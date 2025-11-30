package main

import (
	"context"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
)

// dialManagement dials a management gRPC endpoint with insecure transport (for demo).
func dialManagement(url string) (*grpc.ClientConn, managementpb.ManagementServiceClient, error) {
	// Allow URLs with or without scheme.
	target := url
	if strings.HasPrefix(url, "https://") || strings.HasPrefix(url, "http://") {
		target = strings.TrimPrefix(strings.TrimPrefix(url, "https://"), "http://")
	}
	conn, err := grpc.Dial(target, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, nil, err
	}
	return conn, managementpb.NewManagementServiceClient(conn), nil
}

// managementHealth performs an IsHealthy call if possible.
func managementHealth(ctx context.Context, client managementpb.ManagementServiceClient) error {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	_, err := client.IsHealthy(ctx, &managementpb.Empty{})
	return err
}
