package main

import (
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
)

// Config keeps minimal client state. It is intentionally small and
// independent of the full NetBird client so we can compile and run
// without external dependencies.
type Config struct {
	ManagementURL string   `json:"management_url"`
	SignalURL     string   `json:"signal_url"`
	SetupKey      string   `json:"setup_key"`
	WgIfaceName   string   `json:"wg_iface_name"`
	WgAddress     string   `json:"wg_address"`
	WgListenPort  int      `json:"wg_listen_port"`
	WgPrivateKey  string   `json:"wg_private_key"`
	Peers         []Peer   `json:"peers"`
	Routes        []string `json:"routes"`
}

// Peer is a tiny placeholder for management-provided peers.
type Peer struct {
	ID         string `json:"id"`
	PublicKey  string `json:"public_key"`
	AllowedIPs string `json:"allowed_ips"`
	Endpoint   string `json:"endpoint"`
}

func defaultConfigPath() (string, error) {
	dir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(dir, "netbird", "mini-config.json"), nil
}

func ensureDir(path string) error {
	return os.MkdirAll(filepath.Dir(path), 0o755)
}

func loadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var cfg Config
	if err := json.Unmarshal(data, &cfg); err != nil {
		return nil, err
	}
	return &cfg, nil
}

func saveConfig(path string, cfg *Config) error {
	if cfg == nil {
		return errors.New("nil config")
	}
	if err := ensureDir(path); err != nil {
		return err
	}
	data, err := json.MarshalIndent(cfg, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(path, data, 0o600)
}

func defaultConfig() *Config {
	return &Config{
		ManagementURL: "https://management.example.com:443",
		SignalURL:     "https://signal.example.com:443",
		WgIfaceName:   "wtnb0",
		WgAddress:     "100.64.0.50/32",
		WgListenPort:  51820,
		WgPrivateKey:  "",
		Peers:         []Peer{},
		Routes:        []string{},
	}
}
