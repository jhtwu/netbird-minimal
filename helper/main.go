package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"
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

	// Phase 1: Simple stub implementation
	// Periodically check for setup key and write demo peers

	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()

	for h.running {
		select {
		case <-ticker.C:
			h.update()
		}
	}
}

func (h *Helper) update() {
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
