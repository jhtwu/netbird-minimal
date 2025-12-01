package main

import (
	"context"
	"fmt"
	"runtime"
	"strings"
	"time"

	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
	"github.com/netbirdio/netbird-minimal/miniclient/encryption"
	"golang.zx2c4.com/wireguard/wgctrl/wgtypes"
)

// syncOnce opens the Sync stream, receives one update, and returns peers/routes.
func (c *ManagementClient) syncOnce() ([]Peer, []string, error) {
	if c.grpc == nil || c.serverKey == (wgtypes.Key{}) || c.priv == (wgtypes.Key{}) {
		return nil, nil, fmt.Errorf("sync missing state")
	}

	meta := &managementpb.PeerSystemMeta{
		Hostname:       hostname(),
		GoOS:           runtime.GOOS,
		NetbirdVersion: "mini",
		OS:             runtime.GOOS,
	}
	syncReq := &managementpb.SyncRequest{Meta: meta}
	enc, err := encryption.EncryptMessage(c.serverKey, c.priv, syncReq)
	if err != nil {
		return nil, nil, fmt.Errorf("encrypt sync request: %w", err)
	}

	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	stream, err := c.grpc.Sync(ctx, &managementpb.EncryptedMessage{
		WgPubKey: c.pub.String(),
		Body:     enc,
		Version:  c.version,
	})
	if err != nil {
		return nil, nil, fmt.Errorf("sync call: %w", err)
	}

	respEnc, err := stream.Recv()
	if err != nil {
		return nil, nil, fmt.Errorf("sync recv: %w", err)
	}

	var resp managementpb.SyncResponse
	if err := encryption.DecryptMessage(c.serverKey, c.priv, respEnc.GetBody(), &resp); err != nil {
		return nil, nil, fmt.Errorf("sync decrypt: %w", err)
	}

	var peers []Peer
	var routes []string
	if nm := resp.GetNetworkMap(); nm != nil {
		for _, rp := range nm.GetRemotePeers() {
			peers = append(peers, Peer{
				ID:         rp.GetWgPubKey(),
				PublicKey:  rp.GetWgPubKey(),
				AllowedIPs: strings.Join(rp.GetAllowedIps(), ","),
				Endpoint:   "", // endpoint is discovered via signal/ICE
			})
		}
		for _, r := range nm.GetRoutes() {
			if r.GetNetwork() != "" {
				routes = append(routes, r.GetNetwork())
			}
		}
		if pc := nm.GetPeerConfig(); pc != nil && pc.GetAddress() != "" && c.cfg != nil {
			c.cfg.WgAddress = pc.GetAddress()
		}
	}
	return peers, routes, nil
}
