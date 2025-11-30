package main

import "fmt"

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

