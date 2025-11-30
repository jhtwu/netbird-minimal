#!/bin/bash
#
# test_cli_workflow.sh - Complete CLI workflow test
#
# This script tests the full workflow:
# 1. Create config with WireGuard key
# 2. Start NetBird (up)
# 3. Check status
# 4. Add a peer
# 5. Check status again
# 6. Stop NetBird (down)
#
# Author: Claude
# Date: 2025-11-30

set -e

CLI="./build/netbird-client"
CONFIG_FILE="/tmp/netbird-test-cli.json"

echo ""
echo "================================================================================"
echo "  NetBird CLI Workflow Test"
echo "================================================================================"
echo ""

# Check sudo
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run as root (use sudo)"
    exit 1
fi

# Step 1: Generate WireGuard key and create config
echo "[Step 1] Creating configuration with WireGuard key..."
PRIVKEY=$(wg genkey)
cat > $CONFIG_FILE << EOF
{
  "wg_iface_name": "wt-test",
  "wg_private_key": "$PRIVKEY",
  "wg_address": "100.64.0.123/16",
  "wg_listen_port": 51821,
  "management_url": "https://api.netbird.io:443",
  "signal_url": "https://signal.netbird.io:443",
  "admin_url": "https://app.netbird.io",
  "peer_id": "test-peer-cli-001"
}
EOF
echo "  Config created at $CONFIG_FILE"
echo "  Interface: wt-test"
echo "  Address: 100.64.0.123/16"
echo ""

# Step 2: Start NetBird (in background)
echo "[Step 2] Starting NetBird client..."
# Start in background and give it a few seconds
$CLI -c $CONFIG_FILE up > /tmp/netbird-cli-test.log 2>&1 &
CLI_PID=$!
sleep 2
# Check if it's still running and interface exists
if ip link show wt-test >/dev/null 2>&1; then
    echo "  NetBird started successfully (PID: $CLI_PID)"
else
    echo "  WARNING: Interface not found, but continuing..."
    cat /tmp/netbird-cli-test.log
fi
echo ""

# Step 3: Check status
echo "[Step 3] Checking NetBird status..."
ip link show wt-test >/dev/null 2>&1 && echo "  Interface wt-test is UP" || echo "  Interface wt-test not found"
echo ""

# Step 4: Add a test peer
echo "[Step 4] Adding test peer..."
PEER_PRIVKEY=$(wg genkey)
PEER_PUBKEY=$(echo $PEER_PRIVKEY | wg pubkey)
$CLI -c $CONFIG_FILE add-peer "$PEER_PUBKEY" "203.0.113.50:51820" "10.10.0.0/24"
echo "  Peer added: $PEER_PUBKEY"
echo ""

# Step 5: Show status
echo "[Step 5] Full status check..."
$CLI -c $CONFIG_FILE status
echo ""

# Step 6: Stop NetBird
echo "[Step 6] Stopping NetBird..."
# Kill the background process first
kill $CLI_PID 2>/dev/null || true
sleep 1
$CLI -c $CONFIG_FILE down
echo ""

# Cleanup
rm -f $CONFIG_FILE

echo "================================================================================"
echo "  CLI Workflow Test Completed!"
echo "================================================================================"
echo ""
