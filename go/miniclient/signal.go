package main

import (
	"context"
	"fmt"
)

// SignalClient is a placeholder that would normally manage offer/answer exchange.
// Here it only logs subscription intent.
type SignalClient struct {
	url string
}

func NewSignalClient(url string) *SignalClient {
	return &SignalClient{url: url}
}

func (c *SignalClient) Subscribe(peerID string) error {
	if peerID == "" {
		return fmt.Errorf("peer id is required")
	}
	fmt.Printf("[signal] subscribe to %s at %s (stub)\n", peerID, c.url)
	return nil
}

// TryGRPC attempts a lightweight Send to validate connectivity.
func (c *SignalClient) TryGRPC() {
	s, err := dialSignal(c.url)
	if err != nil {
		fmt.Printf("[signal] dial failed (%v), staying in stub mode\n", err)
		return
	}
	defer s.Close()
	_ = s.Ping(context.Background())
	fmt.Println("[signal] gRPC ping attempted (response may be empty if server expects encryption)")
}
