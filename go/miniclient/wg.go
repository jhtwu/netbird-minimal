package main

import (
	"fmt"
	"os"
	"os/exec"
	"strings"
)

// WGApplier uses ip/wg CLI (prototype-style) to apply a WireGuard config.
// If not running as root, it will log and skip system changes so the CLI
// can still be exercised without privileges.
type WGApplier struct{}

func NewWGApplier() *WGApplier {
	return &WGApplier{}
}

func isRoot() bool {
	return os.Geteuid() == 0
}

func runCmd(name string, args ...string) error {
	cmd := exec.Command(name, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

// generatePrivateKey invokes `wg genkey`.
func generatePrivateKey() (string, error) {
	out, err := exec.Command("wg", "genkey").Output()
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(out)), nil
}

// apply peers/routes using ip/wg commands.
func (w *WGApplier) Apply(cfg *Config, peers []Peer, routes []string) error {
	if cfg.WgIfaceName == "" {
		cfg.WgIfaceName = "wtnb0"
	}
	if cfg.WgListenPort == 0 {
		cfg.WgListenPort = 51820
	}
	// Generate key if absent
	if cfg.WgPrivateKey == "" {
		key, err := generatePrivateKey()
		if err != nil {
			return fmt.Errorf("generate wg private key: %w", err)
		}
		cfg.WgPrivateKey = key
	}

	if !isRoot() {
		fmt.Println("[wg] not root; skipping system apply (config saved only)")
		return nil
	}

	// Create interface if missing
	if err := runCmd("ip", "link", "add", "dev", cfg.WgIfaceName, "type", "wireguard"); err != nil {
		// ignore if already exists
		fmt.Printf("[wg] link add may have failed (possibly exists): %v\n", err)
	}

	// Assign address
	if cfg.WgAddress != "" {
		_ = runCmd("ip", "address", "add", cfg.WgAddress, "dev", cfg.WgIfaceName)
	}

	// Write private key to temp file
	tmp, err := os.CreateTemp("", "nb-wg-key-*")
	if err != nil {
		return err
	}
	defer os.Remove(tmp.Name())
	if _, err := tmp.WriteString(cfg.WgPrivateKey); err != nil {
		tmp.Close()
		return err
	}
	tmp.Close()

	// Set key + port
	if err := runCmd("wg", "set", cfg.WgIfaceName, "private-key", tmp.Name(), "listen-port", fmt.Sprint(cfg.WgListenPort)); err != nil {
		return fmt.Errorf("wg set: %w", err)
	}

	// Peers
	for _, p := range peers {
		args := []string{"set", cfg.WgIfaceName, "peer", p.PublicKey}
		if p.AllowedIPs != "" {
			args = append(args, "allowed-ips", p.AllowedIPs)
		}
		if p.Endpoint != "" {
			args = append(args, "endpoint", p.Endpoint, "persistent-keepalive", "25")
		}
		if err := runCmd("wg", args...); err != nil {
			return fmt.Errorf("wg set peer %s: %w", p.ID, err)
		}
	}

	// Bring up
	if err := runCmd("ip", "link", "set", "dev", cfg.WgIfaceName, "up"); err != nil {
		return fmt.Errorf("ip link set up: %w", err)
	}

	// Routes
	for _, r := range routes {
		_ = runCmd("ip", "route", "add", r, "dev", cfg.WgIfaceName)
	}

	return nil
}

// Teardown deletes the interface (routes removed implicitly).
func (w *WGApplier) Teardown(cfg *Config) error {
	if !isRoot() {
		return nil
	}
	if cfg.WgIfaceName == "" {
		return nil
	}
	return runCmd("ip", "link", "del", "dev", cfg.WgIfaceName)
}
