// +build ignore

package main

import (
	"encoding/base64"
	"fmt"
	"log"
	
	"google.golang.org/protobuf/proto"
	managementpb "github.com/netbirdio/netbird-minimal/proto/management/proto"
)

func main() {
	// Test our encryption
	ourPrivKeyStr := "+PmuysE2mAJRDVPsBDtpYzhD1tTjeG1CIv0eA9geLGE="
	serverPubKeyStr := "yfovLJbRmwYw33Ek6E49z8C3mEvFmAAAA" // Truncated from logs
	
	// Create a simple LoginRequest
	loginReq := &managementpb.LoginRequest{
		SetupKey: "E7807664-D952-4A31-93FB-F090BAA707DA",
		Meta: &managementpb.PeerSystemMeta{
			Hostname: "test",
		},
	}
	
	// Marshal
	loginReqBytes, err := proto.Marshal(loginReq)
	if err != nil {
		log.Fatalf("Marshal error: %v", err)
	}
	
	fmt.Printf("Plaintext size: %d bytes\n", len(loginReqBytes))
	fmt.Printf("Plaintext (hex): %x\n", loginReqBytes[:min(32, len(loginReqBytes))])
	
	// Test encryption
	encrypted, err := EncryptForServer(loginReqBytes, ourPrivKeyStr, serverPubKeyStr)
	if err != nil {
		log.Fatalf("Encrypt error: %v", err)
	}
	
	fmt.Printf("Encrypted size: %d bytes\n", len(encrypted))
	fmt.Printf("Encrypted (base64): %s\n", base64.StdEncoding.EncodeToString(encrypted[:min(64, len(encrypted))]))
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
