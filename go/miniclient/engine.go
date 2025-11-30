package main

import (
	"fmt"
	"time"
)

// Engine wires together the stub management and signal clients and
// simulates applying the received configuration. It does not touch the
// real system; instead, it logs steps to make the flow observable.
type Engine struct {
	cfg        *Config
	mgmt       *ManagementClient
	signal     *SignalClient
	started    bool
	lastSync   time.Time
	peerConfig []Peer
	wg         *WGApplier
}

func NewEngine(cfg *Config, mgmt *ManagementClient, sig *SignalClient) *Engine {
	return &Engine{
		cfg:    cfg,
		mgmt:   mgmt,
		signal: sig,
		wg:     NewWGApplier(),
	}
}

func (e *Engine) Start() error {
	if e.started {
		return nil
	}
	if e.mgmt == nil || e.signal == nil {
		return fmt.Errorf("engine missing clients")
	}
	peers, routes, err := e.mgmt.Register(e.cfg.SetupKey)
	if err != nil {
		return fmt.Errorf("management register failed: %w", err)
	}
	e.peerConfig = peers
	e.cfg.Peers = peers
	e.cfg.Routes = routes
	e.lastSync = time.Now()
	e.started = true

	if err := e.wg.Apply(e.cfg, peers, routes); err != nil {
		return fmt.Errorf("apply wireguard config: %w", err)
	}

	fmt.Printf("[engine] started\n")
	fmt.Printf("[engine] iface=%s addr=%s port=%d\n", e.cfg.WgIfaceName, e.cfg.WgAddress, e.cfg.WgListenPort)
	fmt.Printf("[engine] peers=%d routes=%d\n", len(peers), len(routes))
	return nil
}

func (e *Engine) Stop() {
	if !e.started {
		return
	}
	_ = e.wg.Teardown(e.cfg)
	e.started = false
	fmt.Printf("[engine] stopped\n")
}

func (e *Engine) Status() {
	state := "stopped"
	if e.started {
		state = "running"
	}
	fmt.Printf("status: %s\n", state)
	fmt.Printf("iface : %s\n", e.cfg.WgIfaceName)
	fmt.Printf("addr  : %s\n", e.cfg.WgAddress)
	fmt.Printf("peers : %d\n", len(e.peerConfig))
	fmt.Printf("routes: %d\n", len(e.cfg.Routes))
	if !e.lastSync.IsZero() {
		fmt.Printf("last sync: %s\n", e.lastSync.Format(time.RFC3339))
	}
}
