# Hybrid Architecture: C Client + Go Helper

**Date**: 2025-12-01
**Status**: Phase 4 Implementation Strategy

## Overview

NetBird minimal C client uses a **hybrid architecture** where:
- **C client** handles WireGuard tunnel management (fast, low-level)
- **Go helper** handles complex network protocols (gRPC, encryption, ICE)

This approach provides:
- ✅ Fast WireGuard operations in C
- ✅ Reuse of Go ecosystem for gRPC/encryption
- ✅ Clean separation of concerns
- ✅ Path to full C implementation later

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    User                                      │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              C Client (netbird-client)                       │
│                                                              │
│  Commands:                                                   │
│    • up --setup-key KEY      Start with registration        │
│    • down                     Stop                           │
│    • status                   Show status                    │
│                                                              │
│  Components:                                                 │
│    • Engine (engine.c)        Coordinates all components    │
│    • WireGuard (wg_iface.c)   Interface management          │
│    • Routes (route.c)         Routing configuration         │
│    • Config (config.c)        JSON config management        │
│                                                              │
└─────────┬───────────────────────────────────────────────────┘
          │
          │ Reads/Writes
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│           Shared Configuration Files                         │
│                                                              │
│  /etc/netbird/config.json:                                   │
│    • WireGuard private key                                   │
│    • Interface configuration                                 │
│    • Management/Signal URLs                                  │
│                                                              │
│  /etc/netbird/peers.json:                                    │
│    • Peer list with public keys                              │
│    • Endpoints (IP:port)                                     │
│    • Allowed IPs                                             │
│                                                              │
│  /etc/netbird/routes.json:                                   │
│    • Network routes                                          │
│                                                              │
└─────────┬───────────────────────────────────────────────────┘
          │
          │ Reads/Writes
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│            Go Helper (netbird-helper)                        │
│                                                              │
│  Runs as daemon, handles:                                    │
│    • gRPC Management Client                                  │
│    • gRPC Signal Client                                      │
│    • Message encryption/decryption                           │
│    • ICE (Pion ICE library)                                  │
│    • Peer discovery                                          │
│                                                              │
│  Updates peers.json when:                                    │
│    • New peers join network                                  │
│    • Peer endpoints change (ICE)                             │
│    • Routes update                                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
          │
          │ gRPC + Encryption
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│            NetBird Infrastructure                            │
│                                                              │
│    • Management Server (gRPC)                                │
│    • Signal Server (gRPC)                                    │
│    • STUN/TURN Servers                                       │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Communication Protocol

### Shared Configuration Format

**config.json** (shared, both read/write):
```json
{
  "WireGuardConfig": {
    "PrivateKey": "...",
    "Address": "100.64.0.100/16",
    "ListenPort": 51820
  },
  "ManagementURL": "https://api.netbird.io:443",
  "SignalURL": "https://signal.netbird.io:443",
  "WgIfaceName": "wtnb0",
  "PeerID": "peer-abc123",
  "SetupKey": "..."
}
```

**peers.json** (Go writes, C reads):
```json
{
  "peers": [
    {
      "id": "peer-xyz789",
      "publicKey": "ABC...=",
      "endpoint": "203.0.113.50:51820",
      "allowedIPs": ["100.64.1.0/24"],
      "keepalive": 25
    }
  ],
  "updatedAt": "2025-12-01T12:34:56Z"
}
```

**routes.json** (Go writes, C reads):
```json
{
  "routes": [
    {
      "network": "10.20.0.0/16",
      "metric": 100
    }
  ],
  "updatedAt": "2025-12-01T12:34:56Z"
}
```

## Workflow

### Initial Setup (C Client)

```bash
# User runs C client with setup key
sudo netbird-client up --setup-key KEY123
```

C Client:
1. Loads/creates config.json
2. Starts netbird-helper daemon (if not running)
3. Waits for netbird-helper to update peers.json
4. Creates WireGuard interface
5. Adds peers from peers.json
6. Adds routes from routes.json
7. Keeps running, monitors for updates

### Registration (Go Helper)

Go Helper Daemon:
1. Reads config.json for setup key
2. Connects to Management server via gRPC
3. Registers with setup key
4. Receives peer list
5. Writes peers to peers.json
6. Writes routes to routes.json
7. Notifies C client (file modification)

### Peer Updates (Go Helper)

Go Helper continuously:
1. Maintains gRPC Sync stream with Management
2. Maintains gRPC ConnectStream with Signal
3. Performs ICE for each peer
4. Updates peers.json when endpoints change
5. C client watches peers.json, updates WireGuard

### Stopping

```bash
sudo netbird-client down
```

C Client:
1. Removes routes
2. Removes WireGuard interface
3. Optionally stops netbird-helper daemon

## File Watching (C Client)

C client uses `inotify` (Linux) to watch for file changes:

```c
// Pseudo-code
int inotify_fd = inotify_init();
inotify_add_watch(inotify_fd, "/etc/netbird/peers.json", IN_MODIFY);

while (running) {
    // Wait for file modification
    if (peers.json modified) {
        // Reload peers.json
        // Update WireGuard peers
    }
}
```

## Advantages

1. **Simple Communication**: Just JSON files, no IPC complexity
2. **Debuggable**: Can manually edit JSON to test
3. **Language Agnostic**: Easy to replace Go helper with Rust/C++ later
4. **Stateless**: C client can restart without losing state
5. **Security**: Files can be chmod 600, owned by root

## Disadvantages

1. **File I/O**: Slower than Unix sockets (acceptable for peer updates)
2. **Atomicity**: Need atomic writes (use temp file + rename)
3. **Race Conditions**: Need file locking (flock)

## Implementation Plan

### Phase 1: Basic Integration
- [x] C client reads peers.json
- [x] Go helper writes peers.json
- [x] File watching in C client
- [ ] Basic Management integration

### Phase 2: Full Features
- [ ] ICE endpoint discovery
- [ ] Signal server integration
- [ ] Dynamic peer updates
- [ ] Route updates

### Phase 3: Production
- [ ] Daemon management (systemd)
- [ ] Proper error handling
- [ ] Logging coordination
- [ ] Health checks

## File Locations

Development:
- Config: `/tmp/netbird-test/config.json`
- Peers: `/tmp/netbird-test/peers.json`
- Routes: `/tmp/netbird-test/routes.json`

Production:
- Config: `/etc/netbird/config.json`
- Peers: `/etc/netbird/peers.json`
- Routes: `/etc/netbird/routes.json`

## Security Considerations

1. **File Permissions**: All files must be root-only (chmod 600)
2. **Atomic Writes**: Use temp file + atomic rename
3. **File Locking**: Use flock() to prevent concurrent access
4. **Validation**: C client must validate JSON before using

## Future: Full C Implementation

This hybrid architecture provides a clean interface. Later, we can:
1. Implement gRPC client in C (using grpc-c)
2. Implement encryption in C (libsodium)
3. Implement ICE in C (libnice)
4. Replace Go helper entirely

The C client interface remains the same!
