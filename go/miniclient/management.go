package main

import (
	"fmt"
	"time"
)

// ManagementClient is a stub that pretends to talk to the management API.
// It synthesizes one peer and one route for demonstration.
type ManagementClient struct {
	url string
}

func NewManagementClient(url string) *ManagementClient {
	return &ManagementClient{url: url}
}

func (c *ManagementClient) Register(setupKey string) ([]Peer, []string, error) {
	if setupKey == "" {
		fmt.Println("[mgmt] setup key missing; returning demo peer only")
	}
	peer := Peer{
		ID:         "peer-" + fmt.Sprint(time.Now().Unix()),
		PublicKey:  "PUBKEY_PLACEHOLDER",
		AllowedIPs: "100.64.0.0/16",
		Endpoint:   "203.0.113.10:51820",
	}
	route := "10.0.0.0/24"
	return []Peer{peer}, []string{route}, nil
}
