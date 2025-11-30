package main

import (
	"flag"
	"fmt"
	"os"
)

// Minimal CLI:
//   go run ./miniclient up --setup-key KEY [--config PATH]
//   go run ./miniclient status [--config PATH]
//   go run ./miniclient down [--config PATH]
func main() {
	cmd := "status"
	if len(os.Args) > 1 {
		cmd = os.Args[1]
	}

	configPathFlag := flag.NewFlagSet("flags", flag.ExitOnError)
	setupKey := configPathFlag.String("setup-key", "", "setup key for registration (required for up)")
	configPath := configPathFlag.String("config", "", "config file path (defaults to user config dir)")

	switch cmd {
	case "up":
		configPathFlag.Parse(os.Args[2:])
		if err := runUp(*configPath, *setupKey); err != nil {
			fmt.Fprintf(os.Stderr, "up error: %v\n", err)
			os.Exit(1)
		}
	case "down":
		configPathFlag.Parse(os.Args[2:])
		if err := runDown(*configPath); err != nil {
			fmt.Fprintf(os.Stderr, "down error: %v\n", err)
			os.Exit(1)
		}
	case "status":
		configPathFlag.Parse(os.Args[2:])
		if err := runStatus(*configPath); err != nil {
			fmt.Fprintf(os.Stderr, "status error: %v\n", err)
			os.Exit(1)
		}
	default:
		fmt.Fprintf(os.Stderr, "unknown command %q (expected up|down|status)\n", cmd)
		os.Exit(1)
	}
}

func loadOrDefault(path string) (*Config, string, error) {
	var cfgPath string
	var err error
	if path != "" {
		cfgPath = path
	} else {
		cfgPath, err = defaultConfigPath()
		if err != nil {
			return nil, "", err
		}
	}
	cfg, err := loadConfig(cfgPath)
	if err != nil {
		// create default if missing
		cfg = defaultConfig()
	}
	return cfg, cfgPath, nil
}

func runUp(path, setupKey string) error {
	cfg, cfgPath, err := loadOrDefault(path)
	if err != nil {
		return err
	}
	if setupKey != "" {
		cfg.SetupKey = setupKey
	}
	mgmt := NewManagementClient(cfg.ManagementURL)
	signal := NewSignalClient(cfg.SignalURL)
	engine := NewEngine(cfg, mgmt, signal)
	if err := engine.Start(); err != nil {
		return err
	}
	if cfg.SetupKey == "" {
		fmt.Println("[warn] setup-key not provided; registration stub will fail")
	}
	if err := saveConfig(cfgPath, cfg); err != nil {
		return fmt.Errorf("save config: %w", err)
	}
	// Subscribe to our own ID (stub) to mirror C flow semantics.
	if len(cfg.Peers) > 0 {
		_ = signal.Subscribe(cfg.Peers[0].ID)
	}
	return nil
}

func runDown(path string) error {
	cfg, cfgPath, err := loadOrDefault(path)
	if err != nil {
		return err
	}
	// Stop is no-op other than logging; clear peers/routes to mimic teardown.
	cfg.Peers = nil
	cfg.Routes = nil
	fmt.Println("[engine] stopped (stub)")
	if err := saveConfig(cfgPath, cfg); err != nil {
		return fmt.Errorf("save config: %w", err)
	}
	return nil
}

func runStatus(path string) error {
	cfg, _, err := loadOrDefault(path)
	if err != nil {
		return err
	}
	engine := NewEngine(cfg, NewManagementClient(cfg.ManagementURL), NewSignalClient(cfg.SignalURL))
	engine.Status()
	return nil
}
