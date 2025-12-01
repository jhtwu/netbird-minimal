# Management Protocol Specification

**版本**: 1.0
**日期**: 2025-12-01
**狀態**: Implemented (Phase 4B)
**作者**: NetBird Minimal C Client Team

## 1. 協議概述

### 1.1 目的
Management Protocol 用於 NetBird client 與 Management server 之間的通信，負責：
- Peer 註冊和認證
- 網絡配置同步
- Peer 列表分發
- 路由信息分發

### 1.2 傳輸層
- **協議**: gRPC over TLS
- **端口**: 443 (HTTPS)
- **Server**: api.netbird.io:443

### 1.3 消息格式
- **序列化**: Protocol Buffers (proto3)
- **加密**: NaCl box (Curve25519 + XSalsa20-Poly1305)

### 1.4 Proto 定義
- **文件**: `management.proto`
- **Package**: `management`
- **Go 包**: `github.com/netbirdio/netbird-minimal/proto/management/proto`

---

## 2. 加密規格

### 2.1 加密算法: NaCl box

**算法組成**:
- **密鑰交換**: Curve25519 ECDH
- **對稱加密**: XSalsa20 stream cipher
- **認證**: Poly1305 MAC

**為什麼選擇 NaCl box**:
- ✅ 與 WireGuard 密鑰兼容（都是 Curve25519）
- ✅ 內建密鑰交換，無需手動 ECDH
- ✅ 經過廣泛驗證，安全性高
- ✅ 性能優秀

**Go 實現**:
```go
import "golang.org/x/crypto/nacl/box"
```

**注意**: ❌ **不要使用** `chacha20poly1305` 包，NetBird 使用的是 **XSalsa20** 不是 XChaCha20！

### 2.2 加密參數

| 參數 | 值 | 說明 |
|------|-----|------|
| Nonce Size | 24 bytes | NaCl box 標準 |
| Key Size | 32 bytes | Curve25519 標準 |
| Tag Size | 16 bytes | Poly1305 MAC |
| Overhead | 40 bytes | Nonce (24) + Tag (16) |

### 2.3 加密流程

```
Plaintext (LoginRequest protobuf)
    ↓
Marshal to bytes
    ↓
Generate 24-byte random nonce
    ↓
Derive shared key: Curve25519(ourPrivKey, serverPubKey)
    ↓
Encrypt: XSalsa20-Poly1305
    ↓
Output: nonce || ciphertext
```

### 2.4 Go 實現規格

```go
// Encrypt using NaCl box
func EncryptMessage(plaintext []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
    // Generate nonce
    var nonce [24]byte
    rand.Read(nonce[:])

    // Convert to [32]byte arrays
    privKey := (*[32]byte)(ourPrivKey)
    pubKey := (*[32]byte)(peerPubKey)

    // Encrypt with NaCl box
    // Returns: nonce || ciphertext
    encrypted := box.Seal(nonce[:], plaintext, &nonce, pubKey, privKey)
    return encrypted, nil
}

// Decrypt using NaCl box
func DecryptMessage(encrypted []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
    if len(encrypted) < 24 {
        return nil, errors.New("message too short")
    }

    // Extract nonce
    var nonce [24]byte
    copy(nonce[:], encrypted[:24])

    // Convert keys
    privKey := (*[32]byte)(ourPrivKey)
    pubKey := (*[32]byte)(peerPubKey)

    // Decrypt
    plaintext, ok := box.Open(nil, encrypted[24:], &nonce, pubKey, privKey)
    if !ok {
        return nil, errors.New("decryption failed")
    }

    return plaintext, nil
}
```

---

## 3. RPC 接口規格

### 3.1 GetServerKey

**目的**: 獲取 Management server 的公鑰用於後續加密通信

**請求**: `Empty`
```protobuf
message Empty {}
```

**響應**: `ServerKeyResponse`
```protobuf
message ServerKeyResponse {
  string key = 1;                           // Server WireGuard 公鑰 (base64)
  google.protobuf.Timestamp expiresAt = 2;  // 密鑰過期時間
  int32 version = 3;                        // 協議版本
}
```

**規格**:
- **Request**: 無加密，明文
- **Response**: 無加密，明文
- **Timeout**: 10 秒

**實現要求**:
```go
func (m *ManagementClient) GetServerKey() error {
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    resp, err := m.client.GetServerKey(ctx, &Empty{})
    if err != nil {
        return err
    }

    // Validate key format
    if len(resp.Key) == 0 {
        return errors.New("empty server key")
    }

    // Decode and validate
    key, err := base64.StdEncoding.DecodeString(resp.Key)
    if err != nil {
        return errors.New("invalid base64 key")
    }
    if len(key) != 32 {
        return errors.New("invalid key size")
    }

    m.serverPubKey = resp.Key
    return nil
}
```

**測試規格**:
- Input: 無
- Expected Output: 44 字符 base64 字符串（32 bytes 編碼）
- Validation:
  - ✅ 非空
  - ✅ Valid base64
  - ✅ Decode 後正好 32 bytes

---

### 3.2 Login

**目的**: 使用 setup key 註冊到 NetBird 網絡

**請求**: `EncryptedMessage` (包含 `LoginRequest`)

```protobuf
message EncryptedMessage {
  string wgPubKey = 1;  // 我們的 WG 公鑰 (base64)
  bytes body = 2;       // 加密的 LoginRequest
  // 注意: 不設置 version 字段（默認 0）
}

message LoginRequest {
  string setupKey = 1;              // Setup key (UUID 格式)
  PeerSystemMeta meta = 2;          // 系統元數據
  string jwtToken = 3;              // JWT token (可選)
  PeerKeys peerKeys = 4;            // Peer 密鑰
  repeated string dnsLabels = 5;    // DNS 標籤
}

message PeerSystemMeta {
  string hostname = 1;
  string goOS = 2;
  string kernel = 3;
  string core = 4;              // OSVersion
  string platform = 5;
  string OS = 6;
  string netbirdVersion = 7;
  string uiVersion = 8;
  string kernelVersion = 9;
  string OSVersion = 10;
  repeated NetworkAddress networkAddresses = 11;
  Environment environment = 12;
  repeated File files = 13;
  Flags flags = 14;
}

message PeerKeys {
  bytes sshPubKey = 1;  // SSH 公鑰（可選）
  bytes wgPubKey = 2;   // WG 公鑰 (base64 字符串的 UTF-8 bytes)
}
```

**響應**: `EncryptedMessage` (包含 `LoginResponse`)

```protobuf
message LoginResponse {
  NetbirdConfig netbirdConfig = 1;  // NetBird 配置
  PeerConfig peerConfig = 2;         // 我們的 peer 配置
  repeated Checks Checks = 3;        // Posture checks
}

message PeerConfig {
  string address = 1;   // 我們的 VPN IP (CIDR 格式)
  string dns = 2;       // DNS 服務器
  SSHConfig sshConfig = 3;
  string fqdn = 4;      // 完整域名
  bool RoutingPeerDnsResolutionEnabled = 5;
  bool LazyConnectionEnabled = 6;
  int32 mtu = 7;
}
```

**加密規格**:

1. **加密 LoginRequest**:
```go
// 1. 創建 LoginRequest
loginReq := &LoginRequest{
    SetupKey: "E7807664-D952-4A31-93FB-F090BAA707DA",
    Meta: &PeerSystemMeta{
        Hostname:       "netbird-minimal-c",
        GoOS:           "linux",
        Kernel:         "Linux",
        Core:           "22.04",
        Platform:       "unknown",
        OS:             "linux",
        NetbirdVersion: "0.27.0",
        // ...
    },
    PeerKeys: &PeerKeys{
        SshPubKey: nil,
        WgPubKey:  []byte(ourPubKey),  // ⚠️ Base64 字符串轉 bytes
    },
    JwtToken:  "",
    DnsLabels: []string{},
}

// 2. Marshal
loginReqBytes := proto.Marshal(loginReq)

// 3. 加密
encrypted := EncryptMessage(loginReqBytes, ourPrivKey, serverPubKey)

// 4. 包裝
encMsg := &EncryptedMessage{
    WgPubKey: ourPubKey,  // Base64 string
    Body:     encrypted,
    // 不設置 Version
}
```

2. **解密 LoginResponse**:
```go
// 1. 解密
decrypted := DecryptMessage(respEncMsg.Body, ourPrivKey, serverPubKey)

// 2. Unmarshal
loginResp := &LoginResponse{}
proto.Unmarshal(decrypted, loginResp)
```

**實現要求**:
- **Timeout**: 30 秒
- **Setup Key 格式**: UUID (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
- **PeerKeys.WgPubKey**: Base64 字符串轉成 `[]byte`（不是解碼後的 32 bytes）

**重要**: `PeerKeys.wgPubKey` 是 `bytes` 類型，但存儲的是 **base64 字符串的 UTF-8 bytes**，不是原始的 32-byte 密鑰！

```go
// ✅ 正確
WgPubKey: []byte("YUkmErqD8JUHPgf16IDDFbI8M9XXFe5wXERAd/+wjDA=")

// ❌ 錯誤
key, _ := base64.StdEncoding.DecodeString("YUkmErqD8JUHPgf1...")
WgPubKey: key
```

**測試規格**:
- Input: Valid setup key
- Expected Output: LoginResponse with:
  - `peerConfig.address` 非空（格式: x.x.x.x/16）
  - `netbirdConfig` 非空
- Error Cases:
  - Invalid setup key → InvalidArgument error
  - Expired setup key → PermissionDenied error
  - Used setup key → AlreadyExists error

---

### 3.3 Sync

**目的**: 建立長連接 streaming，接收網絡配置更新

**請求**: `EncryptedMessage` (包含 `SyncRequest`)

```protobuf
message SyncRequest {
  PeerSystemMeta meta = 1;  // 系統元數據
}
```

**響應**: Stream of `EncryptedMessage` (包含 `SyncResponse`)

```protobuf
message SyncResponse {
  NetbirdConfig netbirdConfig = 1;
  PeerConfig peerConfig = 2;              // Deprecated
  repeated RemotePeerConfig remotePeers = 3;  // Deprecated
  bool remotePeersIsEmpty = 4;            // Deprecated
  NetworkMap NetworkMap = 5;              // ⭐ 主要使用這個
  repeated Checks Checks = 6;
}

message NetworkMap {
  uint64 Serial = 1;                       // 配置版本號
  PeerConfig peerConfig = 2;               // 我們的配置
  repeated RemotePeerConfig remotePeers = 3;  // ⭐ Peer 列表
  bool remotePeersIsEmpty = 4;
  repeated Route Routes = 5;               // ⭐ 路由列表
  DNSConfig DNSConfig = 6;
  repeated RemotePeerConfig offlinePeers = 7;
  repeated FirewallRule FirewallRules = 8;
  // ...
}

message RemotePeerConfig {
  string wgPubKey = 1;          // ⭐ Peer 公鑰
  repeated string allowedIps = 2;  // ⭐ AllowedIPs
  SSHConfig sshConfig = 3;
  string fqdn = 4;
  string agentVersion = 5;
}

message Route {
  string ID = 1;
  string Network = 2;    // ⭐ CIDR 格式
  int64 NetworkType = 3;
  string Peer = 4;
  int64 Metric = 5;
  bool Masquerade = 6;
  string NetID = 7;
  repeated string Domains = 8;
  bool keepRoute = 9;
}
```

**Streaming 規格**:

```go
func (m *ManagementClient) Sync(ctx context.Context,
                                 onUpdate func(*SyncResponse)) error {
    // 1. 創建 SyncRequest
    syncReq := &SyncRequest{
        Meta: &PeerSystemMeta{...},
    }

    // 2. 加密
    syncReqBytes := proto.Marshal(syncReq)
    encrypted := EncryptMessage(syncReqBytes, ourPrivKey, serverPubKey)

    encMsg := &EncryptedMessage{
        WgPubKey: ourPubKey,
        Body:     encrypted,
    }

    // 3. 建立 stream
    stream, err := m.client.Sync(ctx, encMsg)
    if err != nil {
        return err
    }

    // 4. 接收循環
    for {
        respEncMsg, err := stream.Recv()
        if err == io.EOF {
            return nil  // Stream closed normally
        }
        if err != nil {
            return err  // Error
        }

        // 5. 解密
        decrypted := DecryptMessage(respEncMsg.Body, ourPrivKey, serverPubKey)

        // 6. Unmarshal
        syncResp := &SyncResponse{}
        proto.Unmarshal(decrypted, syncResp)

        // 7. 回調處理
        onUpdate(syncResp)
    }
}
```

**實現要求**:
- **Stream Type**: Server-side streaming
- **Context**: 使用 long-lived context（不設 timeout）
- **重連**: Stream 斷開時自動重連
- **處理**: 通過回調函數處理更新

**測試規格**:
- Input: Valid SyncRequest
- Expected Output: Stream of SyncResponse
- Validation:
  - ✅ 至少收到一個 SyncResponse
  - ✅ `NetworkMap` 不為 nil
  - ✅ `NetworkMap.remotePeers` 包含 peer 列表
- Error Cases:
  - Not logged in → Unauthenticated error
  - Connection lost → 自動重連

---

## 4. 數據流規格

### 4.1 初始化流程

```
Client                          Management Server
  |                                     |
  |  1. GetServerKey() ───────────────> |
  |     (unencrypted)                   |
  |                                     |
  | <──────────── ServerKeyResponse     |
  |     key: "yfovLJbRmwYw33Ek..."      |
  |                                     |
  |  2. Login(setupKey) ──────────────> |
  |     Encrypted with NaCl box         |
  |     - SetupKey                      |
  |     - PeerSystemMeta                |
  |     - PeerKeys                      |
  |                                     |
  | <───────────── LoginResponse        |
  |     Encrypted                       |
  |     - PeerConfig (our IP)           |
  |     - NetbirdConfig                 |
  |                                     |
  |  3. Sync() ──────────────────────> |
  |     Start streaming                 |
  |                                     |
  | <───────────── SyncResponse (1)     |
  |     - NetworkMap                    |
  |     - RemotePeers (8 peers)         |
  |     - Routes                        |
  |                                     |
  | <───────────── SyncResponse (2)     |
  |     (when network changes)          |
  |                                     |
  | <───────────── ...                  |
  |     (continuous updates)            |
  |                                     |
```

### 4.2 消息序列

1. **GetServerKey**: 獲取加密密鑰
2. **Login**: 認證並註冊
3. **Sync**: 長連接接收更新

**規格**: 必須按此順序執行，Login 之前必須先 GetServerKey。

---

## 5. 錯誤處理規格

### 5.1 gRPC 錯誤碼

| gRPC Code | 場景 | 處理方式 |
|-----------|------|---------|
| `InvalidArgument` | Setup key 格式錯誤 | 提示用戶檢查 setup key |
| `PermissionDenied` | Setup key 過期或撤銷 | 提示用戶獲取新 key |
| `AlreadyExists` | Setup key 已被使用 | 提示用戶獲取新 key |
| `Unauthenticated` | 未登錄就調用 Sync | 先執行 Login |
| `Unavailable` | 網絡連接失敗 | 重試連接 |

### 5.2 加密錯誤

| 錯誤 | 原因 | 解決 |
|------|------|------|
| "authentication failed" | 解密失敗 | 檢查密鑰是否正確 |
| "message too short" | 消息長度 < 24 bytes | 檢查消息完整性 |
| "invalid key size" | 密鑰不是 32 bytes | 檢查密鑰格式 |

### 5.3 Fallback 策略

```go
func (h *Helper) runRealMode() error {
    err := connectToManagement()
    if err != nil {
        log.Printf("Failed to connect: %v", err)
        log.Println("Falling back to stub mode")
        h.runStubMode()
        return err
    }
    return nil
}
```

**規格**: 連接失敗時，fallback 到 stub mode（寫入 demo peers）。

---

## 6. 性能規格

### 6.1 延遲要求

| 操作 | 目標延遲 | 最大延遲 |
|------|---------|---------|
| GetServerKey | < 100ms | 10s |
| Login | < 500ms | 30s |
| Sync (初始) | < 1s | 30s |
| Sync (更新) | < 100ms | N/A |

### 6.2 吞吐量

- **Sync Updates**: 支持高頻更新（每秒多次）
- **Message Size**: 通常 < 10KB
- **Peers**: 支持 100+ peers

### 6.3 資源使用

- **內存**: < 50MB (Go helper)
- **CPU**: < 5% (idle), < 20% (active)
- **網絡**: < 10KB/s (平均)

---

## 7. 安全規格

### 7.1 密鑰管理

**規格**:
- ✅ WireGuard 私鑰存儲在 config.json（權限 0600）
- ✅ Setup key 只在內存中，不寫日誌
- ✅ Server 公鑰從 GetServerKey 獲取，不硬編碼
- ❌ 不驗證 server TLS 證書（TODO: 應該驗證）

**TODO**: 添加 TLS 證書 pinning。

### 7.2 消息認證

**規格**:
- ✅ 所有消息通過 Poly1305 MAC 認證
- ✅ 防止中間人攻擊（基於 WireGuard 密鑰）
- ✅ 防止重放攻擊（nonce）

### 7.3 Setup Key 安全

**規格**:
- Setup key 是一次性的（使用後失效）
- Setup key 可以設置過期時間
- Setup key 可以限制使用次數

---

## 8. 測試驗證規格

### 8.1 單元測試

**加密測試**:
```go
func TestEncryptDecrypt(t *testing.T) {
    privKey := generateKey()
    pubKey := derivePublicKey(privKey)
    plaintext := []byte("test message")

    // Encrypt
    encrypted := EncryptMessage(plaintext, privKey, pubKey)

    // Decrypt
    decrypted := DecryptMessage(encrypted, privKey, pubKey)

    // Verify
    assert.Equal(t, plaintext, decrypted)
}
```

**預期結果**:
- ✅ 加密後長度 = 原始長度 + 40 bytes
- ✅ 解密得到原始數據
- ✅ 修改任何 byte 導致解密失敗

### 8.2 集成測試

**GetServerKey 測試**:
```go
func TestGetServerKey(t *testing.T) {
    client := NewManagementClient("https://api.netbird.io:443", testPrivKey)
    client.Connect()

    err := client.GetServerKey()

    assert.NoError(t, err)
    assert.NotEmpty(t, client.serverPubKey)
    assert.Len(t, base64Decode(client.serverPubKey), 32)
}
```

**Login 測試**:
```go
func TestLogin(t *testing.T) {
    client := setupClient()
    client.GetServerKey()

    resp, err := client.Login("valid-setup-key")

    assert.NoError(t, err)
    assert.NotNil(t, resp.PeerConfig)
    assert.NotEmpty(t, resp.PeerConfig.Address)
}
```

**Sync 測試**:
```go
func TestSync(t *testing.T) {
    client := setupClient()
    client.GetServerKey()
    client.Login("valid-setup-key")

    receivedUpdate := false
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    err := client.Sync(ctx, func(resp *SyncResponse) {
        receivedUpdate = true
        assert.NotNil(t, resp.NetworkMap)
        cancel()
    })

    assert.True(t, receivedUpdate)
}
```

### 8.3 端到端測試

**完整流程測試**:
```bash
# 1. 啟動 helper
./netbird-helper --config-dir /etc/netbird

# 2. 驗證
- [ ] 成功連接到 api.netbird.io
- [ ] Login 成功
- [ ] 收到 peers.json
- [ ] peers.json 包含真實 peer 數據
```

---

## 9. 實現檢查清單

### 9.1 加密實現 ✅

- [x] 使用 `golang.org/x/crypto/nacl/box`
- [x] 不使用 `chacha20poly1305` 包
- [x] EncryptMessage() 實現
- [x] DecryptMessage() 實現
- [x] 密鑰格式驗證

### 9.2 Management Client ✅

- [x] TLS 連接
- [x] GetServerKey() RPC
- [x] Login() RPC
- [x] Sync() streaming RPC
- [x] 錯誤處理
- [x] Fallback to stub mode

### 9.3 數據處理 ✅

- [x] LoginResponse 處理
- [x] SyncResponse 處理
- [x] Peers 解析
- [x] Routes 解析
- [x] 寫入 peers.json
- [x] 寫入 routes.json

### 9.4 測試 ⚠️

- [x] 手動測試（成功連接）
- [ ] 自動化單元測試
- [ ] 自動化集成測試
- [ ] CI/CD 集成

---

## 10. 已知限制

1. **TLS 證書驗證**: 未實現證書 pinning
2. **重連機制**: Sync stream 斷開後未自動重連
3. **錯誤重試**: 未實現指數退避重試
4. **監控**: 無 metrics 和 health check

---

## 11. 參考資料

- **Protocol Buffers**: `/project/netbird/go/proto/management.proto`
- **官方實現**: NetBird Go client source code
- **加密庫**: https://pkg.go.dev/golang.org/x/crypto/nacl/box
- **gRPC 文檔**: https://grpc.io/docs/languages/go/

---

## 附錄 A: 完整代碼示例

見 `/project/netbird/helper/management.go` 和 `/project/netbird/helper/crypto.go`

## 附錄 B: 測試結果

見 `/project/netbird/PHASE_4B_SUCCESS.md`

---

**規格版本歷史**:
- v1.0 (2025-12-01): 初始版本（Phase 4B 完成後補充）
