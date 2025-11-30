package main

import (
	"context"
	"fmt"
	"time"

	"google.golang.org/grpc"

	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
)

// ManagementClient is a stub that pretends to talk to the management API.
// It synthesizes one peer and one route for demonstration.
type ManagementClient struct {
	url      string
	conn     *grpc.ClientConn
	grpc     managementpb.ManagementServiceClient
}

func NewManagementClient(url string) *ManagementClient {
	return &ManagementClient{url: url}
}

func (c *ManagementClient) Register(setupKey string) ([]Peer, []string, error) {
	if c.grpc == nil {
		conn, client, err := dialManagement(c.url)
		if err == nil {
			c.conn = conn
			c.grpc = client
			// fire-and-forget health check
			_ = managementHealth(context.Background(), c.grpc)
		} else {
			fmt.Printf("[mgmt] dial failed (%v), falling back to stub\n", err)
		}
	}

	if setupKey == "" {
		fmt.Println("[mgmt] setup key missing; returning demo peer only")
	}

	// If grpc is available, attempt GetServerKey just to exercise the API.
	if c.grpc != nil {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		_, _ = c.grpc.GetServerKey(ctx, &managementpb.Empty{})
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

func (c *ManagementClient) Close() {
	if c.conn != nil {
		_ = c.conn.Close()
	}
}
