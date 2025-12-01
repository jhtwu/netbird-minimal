package main

import (
	"context"
	"fmt"
	"os"
	"runtime"
	"time"

	"google.golang.org/grpc"

	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
	"github.com/netbirdio/netbird-minimal/miniclient/encryption"
	"golang.zx2c4.com/wireguard/wgctrl/wgtypes"
)

// ManagementClient is a stub that pretends to talk to the management API.
// It synthesizes one peer and one route for demonstration.
type ManagementClient struct {
	url      string
	cfg      *Config
	conn     *grpc.ClientConn
	grpc     managementpb.ManagementServiceClient
	serverKey wgtypes.Key
	version   int32
	priv      wgtypes.Key
	pub       wgtypes.Key
}

func NewManagementClient(url string, cfg *Config) *ManagementClient {
	return &ManagementClient{url: url, cfg: cfg}
}

func (c *ManagementClient) Register(setupKey string) ([]Peer, []string, error) {
	if c.grpc == nil {
		conn, client, err := dialManagement(c.url)
		if err == nil {
			c.conn = conn
			c.grpc = client
			// fire-and-forget health check
			_ = managementHealth(context.Background(), c.grpc)
		} else {
			fmt.Printf("[mgmt] dial failed (%v), falling back to stub\n", err)
		}
	}

	if setupKey == "" {
		fmt.Println("[mgmt] setup key missing; returning demo peer only")
	}

	if c.grpc == nil {
		// no grpc, return stub
		p, r := fallbackStub()
		return p, r, nil
	}

	// Fetch server key
	ctx, cancel := context.WithTimeout(context.Background(), 8*time.Second)
	defer cancel()
	serverKeyResp, err := c.grpc.GetServerKey(ctx, &managementpb.Empty{})
	if err != nil {
		fmt.Printf("[mgmt] get server key failed (%v), falling back to stub\n", err)
		p, r := fallbackStub()
		return p, r, nil
	}
	serverKey, err := wgtypes.ParseKey(serverKeyResp.GetKey())
	if err != nil {
		return nil, nil, fmt.Errorf("parse server key: %w", err)
	}
	c.serverKey = serverKey
	c.version = serverKeyResp.GetVersion()

	// Ensure we have a private key
	priv, pub, err := ensureKey(c.cfg)
	if err != nil {
		return nil, nil, fmt.Errorf("ensure key: %w", err)
	}

	loginReq := &managementpb.LoginRequest{
		SetupKey: setupKey,
		PeerKeys: &managementpb.PeerKeys{
			WgPubKey: []byte(pub.String()),
		},
		Meta: &managementpb.PeerSystemMeta{
			Hostname:       "netbird-minimal-go",
			GoOS:           runtime.GOOS,
			NetbirdVersion: "mini",
			OS:             runtime.GOOS,
		},
	}

	encBody, err := encryption.EncryptMessage(serverKey, priv, loginReq)
	if err != nil {
		return nil, nil, fmt.Errorf("encrypt login: %w", err)
	}

	loginRespEnc, err := c.grpc.Login(ctx, &managementpb.EncryptedMessage{
		WgPubKey: pub.String(),
		Body:     encBody,
		Version:  serverKeyResp.GetVersion(),
	})
	if err != nil {
		return nil, nil, fmt.Errorf("login: %w", err)
	}

	var loginResp managementpb.LoginResponse
	if err := encryption.DecryptMessage(serverKey, priv, loginRespEnc.GetBody(), &loginResp); err != nil {
		return nil, nil, fmt.Errorf("decrypt login: %w", err)
	}
	fmt.Println("[mgmt] login success (encrypted)")
	c.serverKey = serverKey
	c.version = serverKeyResp.GetVersion()
	c.priv, c.pub = priv, pub

	if cfg := c.cfg; cfg != nil {
		if pc := loginResp.GetPeerConfig(); pc != nil && pc.GetAddress() != "" {
			cfg.WgAddress = pc.GetAddress()
		}
		if nc := loginResp.GetNetbirdConfig(); nc != nil && nc.GetSignal().GetUri() != "" {
			cfg.SignalURL = nc.GetSignal().GetUri()
		}
		if cfg.WgListenPort == 0 {
			cfg.WgListenPort = 51820
		}
	}

	// Try one Sync to fetch peers/routes
	peers, routes, err := c.syncOnce()
	if err != nil {
		fmt.Printf("[mgmt] sync failed (%v), continuing without peers/routes\n", err)
	}
	return peers, routes, nil
}

func (c *ManagementClient) Close() {
	if c.conn != nil {
		_ = c.conn.Close()
	}
}

func fallbackStub() ([]Peer, []string) {
	return []Peer{{
		ID:         "peer-" + fmt.Sprint(time.Now().Unix()),
		PublicKey:  "PUBKEY_PLACEHOLDER",
		AllowedIPs: "100.64.0.0/16",
		Endpoint:   "203.0.113.10:51820",
	}}, []string{"10.0.0.0/24"}
}

func ensureKey(cfg *Config) (wgtypes.Key, wgtypes.Key, error) {
	if cfg.WgPrivateKey == "" {
		k, err := wgtypes.GeneratePrivateKey()
		if err != nil {
			return wgtypes.Key{}, wgtypes.Key{}, err
		}
		cfg.WgPrivateKey = k.String()
	}
	priv, err := wgtypes.ParseKey(cfg.WgPrivateKey)
	if err != nil {
		return wgtypes.Key{}, wgtypes.Key{}, err
	}
	pub := priv.PublicKey()
	return priv, pub, nil
}

func hostname() string {
	h, err := os.Hostname()
	if err != nil {
		return "unknown"
	}
	return h
}
