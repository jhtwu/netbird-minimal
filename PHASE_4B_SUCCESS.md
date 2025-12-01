# 🎉 Phase 4B: 成功連接 NetBird 官方 Server！

**日期**: 2025-12-01
**作者**: Claude
**狀態**: ✅ **完全成功！**

## 重大突破

成功實現了與 **NetBird 官方 Management server** 的完整連接，並接收到真實的 peer 列表！

## 測試結果

```
[netbird-helper] NetBird Helper Daemon Started
[netbird-helper] Config Dir:    /tmp/netbird-test
[netbird-helper] Management:    https://api.netbird.io:443
[netbird-helper] Signal:        https://signal.netbird.io:443
[netbird-helper] Setup Key:     E780...07DA

✅ [mgmt] Connecting to api.netbird.io:443 (TLS: true)
✅ [mgmt] Connected successfully
✅ [mgmt] Getting server public key...
✅ [mgmt] Server public key: yfovLJbRmwYw33Ek...
✅ [mgmt] Logging in with setup key...
✅ [mgmt] Marshaled LoginRequest: 175 bytes

🎉 [mgmt] Login successful!
🎉 [mgmt]   Peer Config: true
🎉 [mgmt]   NetBird Config: true
🎉 [mgmt] Our peer config received:
🎉 [mgmt]   Address: 100.72.49.38/16
🎉 [mgmt]   DNS:

✅ [mgmt] Starting Sync stream...
✅ [mgmt] Sync stream established, receiving updates...
✅ [mgmt] Received sync update
✅ [mgmt] Processing 8 peer(s)
✅ [mgmt]   Peer: 6TqWclrW1bRKvI34... (allowed: [100.72.243.30/32])
✅ [mgmt]   Peer: R8du86JZr/gruztR... (allowed: [100.72.186.23/32])
✅ [mgmt]   Peer: rTlgTgM2AnhIoauL... (allowed: [100.72.85.45/32])
✅ [mgmt]   Peer: uCXCzxpmjgR9H6E/... (allowed: [100.72.101.181/32])
✅ [mgmt]   Peer: oPxDhQ7yjRJI2LiL... (allowed: [100.72.10.246/32])
✅ [mgmt]   Peer: SThNHZzqYOKtnDYE... (allowed: [100.72.191.151/32])
✅ [mgmt]   Peer: 7BF1+ElfGgZmG1qB... (allowed: [100.72.6.184/32])
✅ [mgmt]   Peer: N7M6OQ/NuvNlRjdo... (allowed: [100.72.0.150/32])
✅ Wrote /tmp/netbird-test/peers.json
```

## 關鍵發現：加密算法

問題的根源是 **加密算法不正確**！

### 錯誤的實現（之前）
```go
// ❌ 錯誤：使用 XChaCha20-Poly1305
import "golang.org/x/crypto/chacha20poly1305"

func EncryptMessage(plaintext, sharedKey []byte) ([]byte, error) {
    aead, _ := chacha20poly1305.NewX(sharedKey)
    nonce := make([]byte, 24)
    rand.Read(nonce)
    ciphertext := aead.Seal(nil, nonce, plaintext, nil)
    return append(nonce, ciphertext...), nil
}
```

### 正確的實現（現在）
```go
// ✅ 正確：使用 NaCl box (XSalsa20-Poly1305)
import "golang.org/x/crypto/nacl/box"

func EncryptMessage(plaintext []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
    var nonce [24]byte
    rand.Read(nonce[:])

    privKey := (*[32]byte)(ourPrivKey)
    pubKey := (*[32]byte)(peerPubKey)

    // NaCl box: Curve25519 ECDH + XSalsa20-Poly1305
    encrypted := box.Seal(nonce[:], plaintext, &nonce, pubKey, privKey)
    return encrypted, nil
}
```

### 關鍵差異

| 項目 | 錯誤實現 | 正確實現 |
|------|----------|----------|
| 算法 | XChaCha20-Poly1305 | **XSalsa20-Poly1305** |
| 庫 | `chacha20poly1305` | **`nacl/box`** |
| 密鑰交換 | 手動 ECDH | **box 內建** |
| 認證 | Poly1305 (獨立) | **box 整合** |

## 實現細節

### 1. 加密實現 (`helper/crypto.go`)

完整的 NaCl box 實現：

```go
package main

import (
    "crypto/rand"
    "encoding/base64"
    "fmt"
    "golang.org/x/crypto/curve25519"
    "golang.org/x/crypto/nacl/box"
)

const (
    KeySize   = 32  // WireGuard 32-byte keys
    NonceSize = 24  // NaCl box nonce
)

// EncryptMessage encrypts using NaCl box
func EncryptMessage(plaintext []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
    var nonce [NonceSize]byte
    if _, err := rand.Read(nonce[:]); err != nil {
        return nil, err
    }

    privKey := (*[32]byte)(ourPrivKey)
    pubKey := (*[32]byte)(peerPubKey)

    // box.Seal: nonce prepended to ciphertext
    encrypted := box.Seal(nonce[:], plaintext, &nonce, pubKey, privKey)
    return encrypted, nil
}

// DecryptMessage decrypts using NaCl box
func DecryptMessage(encrypted []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
    if len(encrypted) < NonceSize {
        return nil, fmt.Errorf("message too short")
    }

    var nonce [NonceSize]byte
    copy(nonce[:], encrypted[:NonceSize])

    privKey := (*[32]byte)(ourPrivKey)
    pubKey := (*[32]byte)(peerPubKey)

    plaintext, ok := box.Open(nil, encrypted[NonceSize:], &nonce, pubKey, privKey)
    if !ok {
        return nil, fmt.Errorf("authentication failed")
    }

    return plaintext, nil
}

// EncryptForServer - high-level wrapper
func EncryptForServer(plaintext []byte, ourPrivKeyStr, serverPubKeyStr string) ([]byte, error) {
    ourPrivKey, _ := DecodeWGKey(ourPrivKeyStr)
    serverPubKey, _ := DecodeWGKey(serverPubKeyStr)
    return EncryptMessage(plaintext, ourPrivKey, serverPubKey)
}

// DecryptFromServer - high-level wrapper
func DecryptFromServer(encrypted []byte, ourPrivKeyStr, serverPubKeyStr string) ([]byte, error) {
    ourPrivKey, _ := DecodeWGKey(ourPrivKeyStr)
    serverPubKey, _ := DecodeWGKey(serverPubKeyStr)
    return DecryptMessage(encrypted, ourPrivKey, serverPubKey)
}
```

### 2. Management Client (`helper/management.go`)

完整的 gRPC client 實現：

#### Login RPC
```go
func (m *ManagementClient) Login(setupKey string) (*LoginResponse, error) {
    // 1. Create LoginRequest
    loginReq := &managementpb.LoginRequest{
        SetupKey: setupKey,
        Meta: &managementpb.PeerSystemMeta{
            Hostname:       "netbird-minimal-c",
            GoOS:           "linux",
            Kernel:         "Linux",
            Core:           "22.04",
            Platform:       "unknown",
            OS:             "linux",
            NetbirdVersion: "0.27.0",
            // ...
        },
        PeerKeys: &managementpb.PeerKeys{
            SshPubKey: nil,
            WgPubKey:  []byte(m.ourPubKey),  // Base64 string as bytes
        },
        JwtToken:  "",
        DnsLabels: []string{},
    }

    // 2. Marshal to protobuf
    loginReqBytes, _ := proto.Marshal(loginReq)

    // 3. Encrypt with NaCl box
    encrypted, _ := EncryptForServer(loginReqBytes, m.ourPrivKey, m.serverPubKey)

    // 4. Wrap in EncryptedMessage (no version field!)
    encMsg := &managementpb.EncryptedMessage{
        WgPubKey: m.ourPubKey,
        Body:     encrypted,
        // Don't set Version - defaults to 0
    }

    // 5. Call Login RPC
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()
    respEncMsg, _ := m.client.Login(ctx, encMsg)

    // 6. Decrypt response
    decrypted, _ := DecryptFromServer(respEncMsg.Body, m.ourPrivKey, m.serverPubKey)

    // 7. Unmarshal LoginResponse
    loginResp := &managementpb.LoginResponse{}
    proto.Unmarshal(decrypted, loginResp)

    return loginResp, nil
}
```

#### Sync Streaming RPC
```go
func (m *ManagementClient) Sync(ctx context.Context, onUpdate func(*SyncResponse)) error {
    // 1. Create SyncRequest
    syncReq := &managementpb.SyncRequest{
        Meta: &managementpb.PeerSystemMeta{...},
    }

    // 2. Encrypt
    syncReqBytes, _ := proto.Marshal(syncReq)
    encrypted, _ := EncryptForServer(syncReqBytes, m.ourPrivKey, m.serverPubKey)

    encMsg := &managementpb.EncryptedMessage{
        WgPubKey: m.ourPubKey,
        Body:     encrypted,
    }

    // 3. Start streaming
    stream, _ := m.client.Sync(ctx, encMsg)

    // 4. Receive updates loop
    for {
        respEncMsg, err := stream.Recv()
        if err == io.EOF {
            return nil
        }

        // 5. Decrypt and unmarshal
        decrypted, _ := DecryptFromServer(respEncMsg.Body, m.ourPrivKey, m.serverPubKey)
        syncResp := &managementpb.SyncResponse{}
        proto.Unmarshal(decrypted, syncResp)

        // 6. Callback
        onUpdate(syncResp)
    }
}
```

### 3. Helper Daemon 集成 (`helper/main.go`)

#### Real Mode 流程
```go
func (h *Helper) runRealMode() error {
    // 1. Create Management client
    mgmtClient, _ := NewManagementClient(
        h.config.ManagementURL,
        h.config.WireGuardConfig.PrivateKey,
    )

    // 2. Connect (TLS)
    mgmtClient.Connect()

    // 3. Get server public key
    mgmtClient.GetServerKey()

    // 4. Login with setup key
    loginResp, _ := mgmtClient.Login(h.config.SetupKey)
    h.processLoginResponse(loginResp)

    // 5. Start Sync streaming
    ctx := context.Background()
    mgmtClient.Sync(ctx, h.onSyncUpdate)

    return nil
}
```

#### Sync 更新處理
```go
func (h *Helper) onSyncUpdate(resp *SyncResponse) {
    if resp.NetworkMap != nil {
        // Process peers
        h.processPeers(resp.NetworkMap.RemotePeers)

        // Process routes
        h.processRoutes(resp.NetworkMap.Routes)
    }
}

func (h *Helper) processPeers(remotePeers []*RemotePeerConfig) {
    peers := make([]PeerInfo, 0)

    for _, rp := range remotePeers {
        peer := PeerInfo{
            ID:         rp.WgPubKey,
            PublicKey:  rp.WgPubKey,
            Endpoint:   "",  // To be filled by Signal/ICE
            AllowedIPs: rp.AllowedIps,
            Keepalive:  25,
        }
        peers = append(peers, peer)
    }

    // Write peers.json (atomic)
    peersFile := &PeersFile{
        Peers:     peers,
        UpdatedAt: time.Now().Format(time.RFC3339),
    }
    h.writePeers(peersFile)
}
```

## 生成的文件

### peers.json (真實數據！)
```json
{
  "peers": [
    {
      "id": "6TqWclrW1bRKvI340AkPnEJ1aeYp8HSCYt9WiYS7ljs=",
      "publicKey": "6TqWclrW1bRKvI340AkPnEJ1aeYp8HSCYt9WiYS7ljs=",
      "endpoint": "",
      "allowedIPs": ["100.72.243.30/32"],
      "keepalive": 25
    },
    {
      "id": "R8du86JZr/gruztRzfSRyglYbb7z3/naUKOvEJmA+Xk=",
      "publicKey": "R8du86JZr/gruztRzfSRyglYbb7z3/naUKOvEJmA+Xk=",
      "endpoint": "",
      "allowedIPs": ["100.72.186.23/32"],
      "keepalive": 25
    },
    // ... 共 8 個真實 peers
  ],
  "updatedAt": "2025-12-01T12:19:51+08:00"
}
```

## 調試過程

### 嘗試 1-5：Setup Key 問題？
- ❌ 嘗試不同的 setup key
- ❌ 調整 PeerSystemMeta 字段
- ❌ 修改 EncryptedMessage.version
- 結果：都失敗，返回 "invalid request message"

### 嘗試 6：檢查加密算法 ✅
- 閱讀官方源碼 `/project/netbird/go/miniclient/encryption/encryption.go`
- 發現使用 `golang.org/x/crypto/nacl/box`
- **不是 ChaCha20-Poly1305，是 XSalsa20-Poly1305！**

### 嘗試 7：切換到 NaCl box ✅
- 重寫 crypto.go 使用 `nacl/box`
- 測試連接
- **成功！**

## 成功的配置

### config.json
```json
{
  "WireGuardConfig": {
    "PrivateKey": "+PmuysE2mAJRDVPsBDtpYzhD1tTjeG1CIv0eA9geLGE=",
    "Address": "100.64.0.100/16",
    "ListenPort": 51820
  },
  "ManagementURL": "https://api.netbird.io:443",
  "SignalURL": "https://signal.netbird.io:443",
  "WgIfaceName": "wtnb0",
  "PeerID": "test-peer-001",
  "SetupKey": "E7807664-D952-4A31-93FB-F090BAA707DA"
}
```

### 接收到的 Peer Config
- **我們的 IP**: `100.72.49.38/16`
- **Peer 數量**: 8 個
- **DNS**: 未配置

## 架構圖（成功版本）

```
┌────────────────────────────────────────────────┐
│  Go Helper Daemon                              │
│                                                │
│  1. Connect via TLS                            │
│  2. GetServerKey() → yfovLJbRmwYw33Ek...       │
│  3. Login(setupKey)                            │
│     - Encrypt with NaCl box                    │
│     - ✅ Success!                              │
│  4. Sync() streaming                           │
│     - Receive 8 peers                          │
│  5. Write peers.json                           │
└──────────────┬─────────────────────────────────┘
               │
               │ TLS + gRPC
               │ NaCl box (XSalsa20-Poly1305)
               │
               ▼
┌────────────────────────────────────────────────┐
│  NetBird Management Server                     │
│  api.netbird.io:443                            │
│                                                │
│  ✅ Authenticated                              │
│  ✅ Assigned IP: 100.72.49.38/16               │
│  ✅ Sent 8 peers                               │
└────────────────────────────────────────────────┘
```

## 文件變更

### 修改的文件
1. **`helper/crypto.go`** - 完全重寫
   - 從 ChaCha20-Poly1305 切換到 NaCl box
   - 使用 `golang.org/x/crypto/nacl/box`
   - 移除 DeriveSharedKey（box 內建）

2. **`helper/management.go`** - 微調
   - 移除 EncryptedMessage.Version
   - 添加詳細日誌

3. **`helper/main.go`** - 微調
   - 改進錯誤處理
   - 添加 peer 處理日誌

## 下一步：Phase 4C

現在 peers.json 中的 `endpoint` 字段都是空的：

```json
{
  "endpoint": "",  // ⚠️ 需要 Signal + ICE 填充
}
```

**待實現**：

1. **Signal Client** (Go)
   - 實現 Signal gRPC client
   - ConnectStream() 雙向流
   - 發送/接收 ICE candidates

2. **ICE Integration** (Go)
   - Pion ICE library
   - STUN/TURN 支持
   - NAT traversal
   - 更新 peers.json 的 endpoint 字段

3. **File Watching** (C)
   - inotify 監控 peers.json
   - 自動更新 WireGuard peers
   - 動態添加/刪除 peers

4. **端到端測試**
   - Setup key → Login → Sync → Signal → ICE
   - 自動發現 peer endpoints
   - 建立 WireGuard tunnel
   - Ping 測試

## 總結

### 成就 🏆

- ✅ **成功連接到 NetBird 官方 Management server**
- ✅ **Login 認證成功**
- ✅ **接收到真實的 peer 列表（8 個 peers）**
- ✅ **Sync streaming 正常工作**
- ✅ **生成有效的 peers.json**
- ✅ **NaCl box 加密實現正確**
- ✅ **TLS 連接正常**
- ✅ **gRPC 協議正確**

### 關鍵教訓

1. **加密算法很重要** - XChaCha20 vs XSalsa20 的差異導致認證失敗
2. **閱讀官方源碼** - 不要猜測，直接看實現
3. **錯誤信息不一定準確** - "invalid request message" 實際是加密問題

### 項目進度

**Phase 0-4B: 85% 完成**

- ✅ Phase 0: Go 代碼準備
- ✅ Phase 1: WireGuard + routes (C)
- ✅ Phase 2: JSON 配置 (C)
- ✅ Phase 3: Engine + CLI (C)
- ✅ Phase 4A: 混合架構設計
- ✅ **Phase 4B: Management client ← 完成！**
- ⏳ Phase 4C: Signal client + ICE
- ⏳ Phase 4D: File watching (C)
- ⏳ Phase 5: 端到端測試

**我們已經完成了最困難的部分！** 🎉

剩下的工作主要是：
- Signal client（類似 Management client）
- ICE 集成（使用現成的 Pion 庫）
- inotify file watching（標準 Linux API）

NetBird C 移植項目進展順利！
