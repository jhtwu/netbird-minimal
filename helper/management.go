package main

import (
	"context"
	"crypto/tls"
	"fmt"
	"io"
	"log"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/proto"

	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
)

type ManagementClient struct {
	url          string
	conn         *grpc.ClientConn
	client       managementpb.ManagementServiceClient
	serverPubKey string
	ourPrivKey   string
	ourPubKey    string
}

func NewManagementClient(url, ourPrivKey string) (*ManagementClient, error) {
	// Derive our public key
	ourPubKey, err := DerivePublicKey(ourPrivKey)
	if err != nil {
		return nil, fmt.Errorf("derive public key: %w", err)
	}

	return &ManagementClient{
		url:        url,
		ourPrivKey: ourPrivKey,
		ourPubKey:  ourPubKey,
	}, nil
}

func (m *ManagementClient) Connect() error {
	// Parse URL
	target := m.url
	useTLS := false

	if strings.HasPrefix(m.url, "https://") {
		target = strings.TrimPrefix(m.url, "https://")
		useTLS = true
	} else if strings.HasPrefix(m.url, "http://") {
		target = strings.TrimPrefix(m.url, "http://")
	}

	log.Printf("[mgmt] Connecting to %s (TLS: %v)", target, useTLS)

	// Choose credentials
	var opts []grpc.DialOption
	if useTLS {
		// Use system TLS certificates
		tlsConfig := &tls.Config{
			InsecureSkipVerify: false, // Verify server certificate
		}
		creds := credentials.NewTLS(tlsConfig)
		opts = append(opts, grpc.WithTransportCredentials(creds))
	} else {
		opts = append(opts, grpc.WithTransportCredentials(insecure.NewCredentials()))
	}

	conn, err := grpc.Dial(target, opts...)
	if err != nil {
		return fmt.Errorf("dial: %w", err)
	}

	m.conn = conn
	m.client = managementpb.NewManagementServiceClient(conn)

	log.Printf("[mgmt] Connected successfully")
	return nil
}

func (m *ManagementClient) Close() error {
	if m.conn != nil {
		return m.conn.Close()
	}
	return nil
}

// GetServerKey fetches the server's public key
func (m *ManagementClient) GetServerKey() error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	log.Printf("[mgmt] Getting server public key...")

	resp, err := m.client.GetServerKey(ctx, &managementpb.Empty{})
	if err != nil {
		return fmt.Errorf("get server key: %w", err)
	}

	m.serverPubKey = resp.Key
	log.Printf("[mgmt] Server public key: %s", m.serverPubKey[:16]+"...")
	return nil
}

// Login registers with the management server using a setup key
func (m *ManagementClient) Login(setupKey string) (*managementpb.LoginResponse, error) {
	if m.serverPubKey == "" {
		return nil, fmt.Errorf("server public key not set, call GetServerKey first")
	}

	log.Printf("[mgmt] Logging in with setup key...")

	// Create LoginRequest
	// Note: Match official client's metadata structure
	loginReq := &managementpb.LoginRequest{
		SetupKey: setupKey,
		Meta: &managementpb.PeerSystemMeta{
			Hostname:       "netbird-minimal-c",
			GoOS:           "linux",
			Kernel:         "Linux",
			Core:           "22.04",          // OSVersion
			Platform:       "unknown",
			OS:             "linux",
			KernelVersion:  "5.15.0-generic",
			OSVersion:      "22.04",
			NetbirdVersion: "0.27.0",
			UiVersion:      "",
			NetworkAddresses: []*managementpb.NetworkAddress{},
			Environment:      nil,
			Files:            []*managementpb.File{},
			Flags:            nil,
		},
		PeerKeys: &managementpb.PeerKeys{
			SshPubKey: nil,                // SSH key not used in minimal client
			WgPubKey:  []byte(m.ourPubKey), // Base64 string as bytes!
		},
		JwtToken:  "",        // No JWT token, using setup key
		DnsLabels: []string{}, // Empty DNS labels
	}

	log.Printf("[mgmt] LoginRequest: setupKey=%s, hostname=%s, wgPubKey=%s",
		setupKey[:8]+"...", loginReq.Meta.Hostname, m.ourPubKey[:16]+"...")

	// Serialize to protobuf
	loginReqBytes, err := proto.Marshal(loginReq)
	if err != nil {
		return nil, fmt.Errorf("marshal login request: %w", err)
	}
	log.Printf("[mgmt] Marshaled LoginRequest: %d bytes", len(loginReqBytes))

	// Encrypt
	encrypted, err := EncryptForServer(loginReqBytes, m.ourPrivKey, m.serverPubKey)
	if err != nil {
		return nil, fmt.Errorf("encrypt login request: %w", err)
	}

	// Create EncryptedMessage
	// Note: Don't set Version field, it defaults to 0 (official client doesn't set it)
	encMsg := &managementpb.EncryptedMessage{
		WgPubKey: m.ourPubKey,
		Body:     encrypted,
	}

	// Call Login
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	respEncMsg, err := m.client.Login(ctx, encMsg)
	if err != nil {
		log.Printf("[mgmt] Login failed with error: %v", err)
		log.Printf("[mgmt] Setup key used: %s", setupKey[:8]+"...")
		log.Printf("[mgmt] Our public key: %s", m.ourPubKey)
		return nil, fmt.Errorf("login call: %w", err)
	}

	// Decrypt response
	decrypted, err := DecryptFromServer(respEncMsg.Body, m.ourPrivKey, m.serverPubKey)
	if err != nil {
		return nil, fmt.Errorf("decrypt login response: %w", err)
	}

	// Parse LoginResponse
	loginResp := &managementpb.LoginResponse{}
	if err := proto.Unmarshal(decrypted, loginResp); err != nil {
		return nil, fmt.Errorf("unmarshal login response: %w", err)
	}

	log.Printf("[mgmt] Login successful!")
	log.Printf("[mgmt]   Peer Config: %v", loginResp.PeerConfig != nil)
	log.Printf("[mgmt]   NetBird Config: %v", loginResp.NetbirdConfig != nil)

	return loginResp, nil
}

// mustDecodeWGKey decodes a WG key or panics (for inline use)
func mustDecodeWGKey(keyStr string) []byte {
	key, err := DecodeWGKey(keyStr)
	if err != nil {
		panic(fmt.Sprintf("failed to decode WG key: %v", err))
	}
	return key
}

// Sync starts a streaming sync with the management server
func (m *ManagementClient) Sync(ctx context.Context, onUpdate func(*managementpb.SyncResponse)) error {
	if m.serverPubKey == "" {
		return fmt.Errorf("server public key not set")
	}

	log.Printf("[mgmt] Starting Sync stream...")

	// Create SyncRequest
	syncReq := &managementpb.SyncRequest{
		Meta: &managementpb.PeerSystemMeta{
			Hostname: "netbird-minimal-c",
			GoOS:     "linux",
			Core:     "minimal",
			Platform: "linux",
			OS:       "Linux",
		},
	}

	// Serialize
	syncReqBytes, err := proto.Marshal(syncReq)
	if err != nil {
		return fmt.Errorf("marshal sync request: %w", err)
	}

	// Encrypt
	encrypted, err := EncryptForServer(syncReqBytes, m.ourPrivKey, m.serverPubKey)
	if err != nil {
		return fmt.Errorf("encrypt sync request: %w", err)
	}

	// Create EncryptedMessage
	// Note: Don't set Version field, it defaults to 0 (official client doesn't set it)
	encMsg := &managementpb.EncryptedMessage{
		WgPubKey: m.ourPubKey,
		Body:     encrypted,
	}

	// Call Sync (streaming)
	stream, err := m.client.Sync(ctx, encMsg)
	if err != nil {
		return fmt.Errorf("sync call: %w", err)
	}

	log.Printf("[mgmt] Sync stream established, receiving updates...")

	// Receive updates
	for {
		respEncMsg, err := stream.Recv()
		if err == io.EOF {
			log.Printf("[mgmt] Sync stream closed by server")
			return nil
		}
		if err != nil {
			return fmt.Errorf("receive sync update: %w", err)
		}

		// Decrypt
		decrypted, err := DecryptFromServer(respEncMsg.Body, m.ourPrivKey, m.serverPubKey)
		if err != nil {
			log.Printf("[mgmt] Failed to decrypt sync response: %v", err)
			continue
		}

		// Parse SyncResponse
		syncResp := &managementpb.SyncResponse{}
		if err := proto.Unmarshal(decrypted, syncResp); err != nil {
			log.Printf("[mgmt] Failed to unmarshal sync response: %v", err)
			continue
		}

		log.Printf("[mgmt] Received sync update")

		// Call handler
		if onUpdate != nil {
			onUpdate(syncResp)
		}
	}
}
