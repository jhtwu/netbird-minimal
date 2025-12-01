package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"

	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
)

// NetBird Helper Daemon
//
// This Go daemon handles the complex gRPC/encryption/ICE parts of NetBird,
// while the C client focuses on WireGuard tunnel management.
//
// Communication: Shared JSON configuration files
//   - Reads: config.json (setup key, URLs, etc.)
//   - Writes: peers.json, routes.json
//
// The C client watches these files and updates WireGuard configuration.

const (
	DefaultConfigDir = "/etc/netbird"
	DefaultLogFile   = "/var/log/netbird-helper.log"
)

type Config struct {
	WireGuardConfig struct {
		PrivateKey string `json:"PrivateKey"`
		Address    string `json:"Address"`
		ListenPort int    `json:"ListenPort"`
	} `json:"WireGuardConfig"`
	ManagementURL string `json:"ManagementURL"`
	SignalURL     string `json:"SignalURL"`
	WgIfaceName   string `json:"WgIfaceName"`
	PeerID        string `json:"PeerID"`
	SetupKey      string `json:"SetupKey"`
}

type PeerInfo struct {
	ID         string   `json:"id"`
	PublicKey  string   `json:"publicKey"`
	Endpoint   string   `json:"endpoint"`
	AllowedIPs []string `json:"allowedIPs"`
	Keepalive  int      `json:"keepalive"`
}

type PeersFile struct {
	Peers     []PeerInfo `json:"peers"`
	UpdatedAt string     `json:"updatedAt"`
}

type RouteInfo struct {
	Network string `json:"network"`
	Metric  int    `json:"metric"`
}

type RoutesFile struct {
	Routes    []RouteInfo `json:"routes"`
	UpdatedAt string      `json:"updatedAt"`
}

type Helper struct {
	configDir  string
	config     *Config
	peersFile  string
	routesFile string
	running    bool
	mgmtClient *ManagementClient
}

func main() {
	configDir := flag.String("config-dir", DefaultConfigDir, "Configuration directory")
	daemon := flag.Bool("daemon", false, "Run as daemon")
	flag.Parse()

	log.SetPrefix("[netbird-helper] ")
	log.SetFlags(log.LstdFlags | log.Lshortfile)

	if *daemon {
		// TODO: Daemonize process
		log.Println("Running as daemon")
	}

	helper := &Helper{
		configDir:  *configDir,
		peersFile:  filepath.Join(*configDir, "peers.json"),
		routesFile: filepath.Join(*configDir, "routes.json"),
		running:    true,
	}

	// Load configuration
	if err := helper.loadConfig(); err != nil {
		log.Fatalf("Failed to load config: %v", err)
	}

	log.Println("========================================")
	log.Println("  NetBird Helper Daemon Started")
	log.Println("========================================")
	log.Printf("  Config Dir:    %s\n", helper.configDir)
	log.Printf("  Management:    %s\n", helper.config.ManagementURL)
	log.Printf("  Signal:        %s\n", helper.config.SignalURL)
	log.Printf("  Setup Key:     %s\n", maskSetupKey(helper.config.SetupKey))
	log.Println("========================================")

	// Setup signal handling
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Start helper goroutine
	go helper.run()

	// Wait for signal
	sig := <-sigChan
	log.Printf("Received signal %v, shutting down...\n", sig)
	helper.stop()
}

func (h *Helper) loadConfig() error {
	configFile := filepath.Join(h.configDir, "config.json")
	data, err := os.ReadFile(configFile)
	if err != nil {
		return fmt.Errorf("read config: %w", err)
	}

	h.config = &Config{}
	if err := json.Unmarshal(data, h.config); err != nil {
		return fmt.Errorf("parse config: %w", err)
	}

	return nil
}

func (h *Helper) run() {
	log.Println("Helper started, entering main loop...")

	// Check if we have a setup key
	if h.config.SetupKey == "" {
		log.Println("[WARN] No setup key provided, using stub mode")
		h.runStubMode()
		return
	}

	// Real mode: Connect to Management server
	log.Println("Running in REAL mode (connecting to Management server)")
	if err := h.runRealMode(); err != nil {
		log.Printf("[ERROR] Real mode failed: %v", err)
		log.Println("Falling back to stub mode...")
		h.runStubMode()
	}
}

func (h *Helper) runStubMode() {
	log.Println("[STUB] Running in stub mode (demo peers)")
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	for h.running {
		select {
		case <-ticker.C:
			h.updateStub()
		}
	}
}

func (h *Helper) runRealMode() error {
	// Create Management client
	mgmtClient, err := NewManagementClient(h.config.ManagementURL, h.config.WireGuardConfig.PrivateKey)
	if err != nil {
		return fmt.Errorf("create management client: %w", err)
	}
	h.mgmtClient = mgmtClient

	// Connect
	if err := mgmtClient.Connect(); err != nil {
		return fmt.Errorf("connect: %w", err)
	}
	defer mgmtClient.Close()

	// Get server public key
	if err := mgmtClient.GetServerKey(); err != nil {
		return fmt.Errorf("get server key: %w", err)
	}

	// Login
	loginResp, err := mgmtClient.Login(h.config.SetupKey)
	if err != nil {
		return fmt.Errorf("login: %w", err)
	}

	// Process initial login response
	h.processLoginResponse(loginResp)

	// Start Sync stream
	ctx := context.Background()
	if err := mgmtClient.Sync(ctx, h.onSyncUpdate); err != nil {
		return fmt.Errorf("sync: %w", err)
	}

	return nil
}

func (h *Helper) processLoginResponse(resp *managementpb.LoginResponse) {
	log.Println("[mgmt] Processing login response...")

	// LoginResponse doesn't contain NetworkMap or peers
	// Peers are received via Sync stream
	if resp.PeerConfig != nil {
		log.Printf("[mgmt] Our peer config received:")
		log.Printf("[mgmt]   Address: %s", resp.PeerConfig.Address)
		log.Printf("[mgmt]   DNS: %s", resp.PeerConfig.Dns)
	}

	log.Println("[mgmt] Login successful, waiting for Sync updates...")
}

func (h *Helper) onSyncUpdate(resp *managementpb.SyncResponse) {
	log.Println("[mgmt] Processing sync update...")

	// Extract peers from NetworkMap
	if resp.NetworkMap != nil {
		h.processPeers(resp.NetworkMap.RemotePeers)
		h.processRoutes(resp.NetworkMap.Routes)
	}
}

func (h *Helper) processPeers(remotePeers []*managementpb.RemotePeerConfig) {
	if len(remotePeers) == 0 {
		log.Println("[mgmt] No remote peers")
		return
	}

	log.Printf("[mgmt] Processing %d peer(s)", len(remotePeers))

	peers := make([]PeerInfo, 0, len(remotePeers))
	for _, rp := range remotePeers {
		peer := PeerInfo{
			ID:         rp.WgPubKey, // Use pubkey as ID for now
			PublicKey:  rp.WgPubKey,
			Endpoint:   "", // Will be filled by Signal/ICE
			AllowedIPs: rp.AllowedIps,
			Keepalive:  25,
		}
		peers = append(peers, peer)
		log.Printf("[mgmt]   Peer: %s (allowed: %v)", peer.PublicKey[:16]+"...", peer.AllowedIPs)
	}

	peersFile := &PeersFile{
		Peers:     peers,
		UpdatedAt: time.Now().Format(time.RFC3339),
	}

	if err := h.writePeers(peersFile); err != nil {
		log.Printf("[ERROR] Failed to write peers: %v", err)
	}
}

func (h *Helper) processRoutes(routes []*managementpb.Route) {
	if len(routes) == 0 {
		log.Println("[mgmt] No routes")
		return
	}

	log.Printf("[mgmt] Processing %d route(s)", len(routes))

	routeInfos := make([]RouteInfo, 0, len(routes))
	for _, r := range routes {
		route := RouteInfo{
			Network: r.Network,
			Metric:  100,
		}
		routeInfos = append(routeInfos, route)
		log.Printf("[mgmt]   Route: %s", route.Network)
	}

	routesFile := &RoutesFile{
		Routes:    routeInfos,
		UpdatedAt: time.Now().Format(time.RFC3339),
	}

	if err := h.writeRoutes(routesFile); err != nil {
		log.Printf("[ERROR] Failed to write routes: %v", err)
	}
}

func (h *Helper) updateStub() {
	// Check if we have a setup key
	if h.config.SetupKey == "" {
		log.Println("No setup key, skipping update")
		return
	}

	log.Println("Updating peer and route configuration...")

	// For now, write demo data
	// TODO: Implement real Management/Signal clients

	// Write demo peers
	peers := PeersFile{
		Peers: []PeerInfo{
			{
				ID:         "peer-demo-001",
				PublicKey:  "DEMO_PEER_PUBLIC_KEY_FROM_GO_HELPER",
				Endpoint:   "203.0.113.100:51820",
				AllowedIPs: []string{"100.64.10.0/24"},
				Keepalive:  25,
			},
		},
		UpdatedAt: time.Now().Format(time.RFC3339),
	}

	if err := h.writePeers(&peers); err != nil {
		log.Printf("Failed to write peers: %v", err)
	}

	// Write demo routes
	routes := RoutesFile{
		Routes: []RouteInfo{
			{
				Network: "10.30.0.0/16",
				Metric:  100,
			},
		},
		UpdatedAt: time.Now().Format(time.RFC3339),
	}

	if err := h.writeRoutes(&routes); err != nil {
		log.Printf("Failed to write routes: %v", err)
	}

	log.Println("Configuration updated successfully")
}

func (h *Helper) writePeers(peers *PeersFile) error {
	return h.writeJSONAtomic(h.peersFile, peers)
}

func (h *Helper) writeRoutes(routes *RoutesFile) error {
	return h.writeJSONAtomic(h.routesFile, routes)
}

func (h *Helper) writeJSONAtomic(path string, data interface{}) error {
	// Atomic write: write to temp file, then rename
	tmpFile := path + ".tmp"

	jsonData, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal json: %w", err)
	}

	if err := os.WriteFile(tmpFile, jsonData, 0600); err != nil {
		return fmt.Errorf("write temp file: %w", err)
	}

	if err := os.Rename(tmpFile, path); err != nil {
		return fmt.Errorf("atomic rename: %w", err)
	}

	log.Printf("Wrote %s", path)
	return nil
}

func (h *Helper) stop() {
	log.Println("Stopping helper...")
	h.running = false
}

func maskSetupKey(key string) string {
	if key == "" {
		return "(none)"
	}
	if len(key) <= 8 {
		return "***"
	}
	return key[:4] + "..." + key[len(key)-4:]
}
