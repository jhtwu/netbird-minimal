package main

import (
	"flag"
	"fmt"
	"os"
	"time"
)

// A tiny placeholder CLI so the Go tree can build and run without pulling in
// the full NetBird client dependencies. It only echoes parsed options.
func main() {
	iface := flag.String("iface", "nbg0", "wireguard interface name (placeholder)")
	addr := flag.String("addr", "100.64.0.10/32", "wireguard address (placeholder)")
	peer := flag.String("peer", "", "peer public key (optional, placeholder)")
	setupKey := flag.String("setup-key", "", "setup key (placeholder, unused)")
	flag.Parse()

	fmt.Printf("NetBird minimal Go stub\n")
	fmt.Printf("  iface     : %s\n", *iface)
	fmt.Printf("  addr      : %s\n", *addr)
	fmt.Printf("  peer      : %s\n", *peer)
	fmt.Printf("  setup-key : %s\n", *setupKey)
	fmt.Printf("  timestamp : %s\n", time.Now().Format(time.RFC3339))

	// Exit explicitly to avoid lingering goroutines if future code is added.
	os.Exit(0)
}
