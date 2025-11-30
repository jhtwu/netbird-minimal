package main

import (
	"context"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	signalpb "github.com/netbirdio/netbird-minimal/proto/signal/proto"
)

type SignalGRPC struct {
	conn   *grpc.ClientConn
	client signalpb.SignalExchangeClient
}

func dialSignal(url string) (*SignalGRPC, error) {
	target := url
	if strings.HasPrefix(url, "https://") || strings.HasPrefix(url, "http://") {
		target = strings.TrimPrefix(strings.TrimPrefix(url, "https://"), "http://")
	}
	conn, err := grpc.Dial(target, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, err
	}
	return &SignalGRPC{conn: conn, client: signalpb.NewSignalExchangeClient(conn)}, nil
}

// Ping sends a single empty EncryptedMessage through Send to validate connectivity.
func (s *SignalGRPC) Ping(ctx context.Context) error {
	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	_, err := s.client.Send(ctx, &signalpb.EncryptedMessage{})
	return err
}

func (s *SignalGRPC) Close() {
	if s.conn != nil {
		_ = s.conn.Close()
	}
}
