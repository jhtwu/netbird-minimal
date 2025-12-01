# Signal Protocol Specification

**版本**: 1.0
**日期**: 2025-12-01
**狀態**: Specification (Phase 4C - Not Yet Implemented)
**作者**: NetBird Minimal C Client Team

## 1. 協議概述

### 1.1 目的
Signal Protocol 用於 NetBird peers 之間的 ICE (Interactive Connectivity Establishment) signaling，負責：
- 交換 ICE candidates（網絡地址候選）
- NAT 穿透協調
- 建立 P2P 連接前的信令交換
- Peer 在線狀態通知

### 1.2 傳輸層
- **協議**: gRPC over TLS
- **端口**: 443 (HTTPS)
- **Server**: signal.netbird.io:443 (或 Management 返回的地址)

### 1.3 消息格式
- **序列化**: Protocol Buffers (proto3)
- **加密**: NaCl box (與 Management 相同)
- **Proto 文件**: `signalexchange.proto`

### 1.4 與 Management 的區別

| 項目 | Management | Signal |
|------|-----------|--------|
| 目的 | 網絡配置管理 | Peer 間信令 |
| 通信模式 | Client ↔ Server | Client ↔ Server ↔ Client |
| 消息類型 | Config, Peers, Routes | ICE candidates, Offers |
| 加密對象 | Client-Server | Peer-to-Peer |

---

## 2. Proto 定義

### 2.1 Signal Service

```protobuf
service SignalExchange {
  // Connect stream - bidirectional streaming
  rpc ConnectStream(stream EncryptedMessage) returns (stream EncryptedMessage) {}

  // Send message to specific peer
  rpc Send(EncryptedMessage) returns (Empty) {}

  // Receive messages
  rpc Receive(stream EncryptedMessage) returns (stream EncryptedMessage) {}
}
```

### 2.2 Message Types

```protobuf
message EncryptedMessage {
  string wgPubKey = 1;     // Sender WG public key
  string remotePubKey = 2; // Recipient WG public key
  bytes body = 3;          // Encrypted message body
}

// Body 內容（解密後）
message Message {
  string type = 1;         // "offer", "answer", "candidate"
  bytes payload = 2;       // Type-specific payload
  string remoteKey = 3;    // Target peer public key
}
```

---

## 3. 加密規格

### 3.1 加密算法
與 Management Protocol 相同：**NaCl box** (Curve25519 + XSalsa20-Poly1305)

### 3.2 密鑰使用

**與 Management 的區別**:
- Management: 加密對象是 **Client ↔ Server**
- Signal: 加密對象是 **Peer ↔ Peer**（通過 Server 中轉）

**規格**:
```
Alice                Signal Server              Bob
  |                        |                      |
  | Encrypt(msg, AlicePriv, BobPub)              |
  | ────────────────────> |                      |
  |                        |                      |
  |                        | Forward(encrypted)   |
  |                        | ──────────────────> |
  |                        |                      |
  |                        | Decrypt(msg, BobPriv, AlicePub)
  |                        |                      |
```

**實現要求**:
```go
// 發送消息給 peer
func SendToPeer(msg []byte, targetPeerPubKey string) {
    // 加密: 用我們的私鑰 + 對方的公鑰
    encrypted := EncryptMessage(msg, ourPrivKey, targetPeerPubKey)

    encMsg := &EncryptedMessage{
        WgPubKey:    ourPubKey,      // 我們的公鑰
        RemotePubKey: targetPeerPubKey, // 對方的公鑰
        Body:        encrypted,
    }

    // 發送到 Signal server
    stream.Send(encMsg)
}
```

---

## 4. RPC 接口規格

### 4.1 ConnectStream (推薦使用)

**目的**: 建立雙向 streaming 連接，用於實時信令交換

**類型**: Bidirectional Streaming RPC

**Request/Response**: `stream EncryptedMessage`

**規格**:

```go
func (s *SignalClient) ConnectStream(ctx context.Context) error {
    // 1. 建立雙向 stream
    stream, err := s.client.ConnectStream(ctx)
    if err != nil {
        return err
    }

    // 2. 發送初始消息（註冊）
    initMsg := &EncryptedMessage{
        WgPubKey: s.ourPubKey,
        // Body 可以為空，僅註冊
    }
    stream.Send(initMsg)

    // 3. 啟動接收 goroutine
    go s.receiveLoop(stream)

    // 4. 保持 stream 引用
    s.stream = stream

    return nil
}

func (s *SignalClient) receiveLoop(stream SignalExchange_ConnectStreamClient) {
    for {
        msg, err := stream.Recv()
        if err == io.EOF {
            return
        }
        if err != nil {
            log.Printf("Receive error: %v", err)
            return
        }

        // 處理接收到的消息
        s.handleMessage(msg)
    }
}
```

**實現要求**:
- **Context**: Long-lived context（不設 timeout）
- **Goroutine**: 接收循環在獨立 goroutine
- **Error Handling**: 連接斷開時重連
- **Buffering**: 使用 channel 緩衝消息

**測試規格**:
- Input: EncryptedMessage stream
- Expected Output: 成功建立連接
- Validation:
  - ✅ Stream 不為 nil
  - ✅ 可以發送消息
  - ✅ 可以接收消息

---

### 4.2 Send (單向發送)

**目的**: 向特定 peer 發送單個消息（非 streaming）

**Request**: `EncryptedMessage`
**Response**: `Empty`

**規格**:
```go
func (s *SignalClient) SendMessage(msg *EncryptedMessage) error {
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()

    _, err := s.client.Send(ctx, msg)
    return err
}
```

**使用場景**:
- 一次性消息發送
- 不需要持續連接
- 簡單的信令交換

**限制**:
- 不適合高頻消息
- 每次都要建立新連接
- 無法接收響應

**推薦**: 使用 ConnectStream 代替。

---

## 5. 消息類型規格

### 5.1 Offer (提議)

**目的**: Alice 向 Bob 發起連接請求

**Payload**: ICE session description

```json
{
  "type": "offer",
  "payload": {
    "type": "offer",
    "sdp": "v=0\r\no=- ... (SDP format)"
  },
  "remoteKey": "Bob_WG_PubKey"
}
```

**規格**:
- Alice 創建 ICE agent
- 生成 offer SDP
- 加密後通過 Signal server 發送給 Bob

---

### 5.2 Answer (應答)

**目的**: Bob 響應 Alice 的連接請求

**Payload**: ICE session description

```json
{
  "type": "answer",
  "payload": {
    "type": "answer",
    "sdp": "v=0\r\no=- ... (SDP format)"
  },
  "remoteKey": "Alice_WG_PubKey"
}
```

**規格**:
- Bob 收到 offer
- 創建 ICE agent（設置 remote description）
- 生成 answer SDP
- 加密後發送給 Alice

---

### 5.3 Candidate (ICE 候選)

**目的**: 交換網絡地址候選（可能的連接路徑）

**Payload**: ICE candidate

```json
{
  "type": "candidate",
  "payload": {
    "candidate": "candidate:1 1 UDP 2130706431 192.168.1.100 51820 typ host",
    "sdpMid": "0",
    "sdpMLineIndex": 0
  },
  "remoteKey": "Peer_WG_PubKey"
}
```

**規格**:
- 每個 ICE candidate 發現後立即發送
- 通常會有多個 candidates:
  - Host candidate (本地 IP)
  - Server reflexive (公網 IP via STUN)
  - Relay candidate (中繼 via TURN)

---

## 6. ICE 流程規格

### 6.1 完整信令流程

```
Alice                    Signal Server              Bob
  |                            |                      |
  | 1. ConnectStream()         |                      |
  | ─────────────────────────> |                      |
  |                            | 2. ConnectStream()   |
  |                            | <──────────────────  |
  |                            |                      |
  | 3. Create ICE Agent        |                      |
  | 4. Generate Offer          |                      |
  |                            |                      |
  | 5. Send(Offer) ──────────> | 6. Forward(Offer) ─>|
  |                            |                      |
  |                            |      7. Create ICE Agent
  |                            |      8. Set Remote Desc
  |                            |      9. Generate Answer
  |                            |                      |
  | <─ 10. Forward(Answer) ──  | <─ Send(Answer)      |
  |                            |                      |
  | 11. Set Remote Desc        |                      |
  |                            |                      |
  | 12. Gather Candidates      |      13. Gather Candidates
  |                            |                      |
  | 14. Send(Candidate) ─────> | 15. Forward ──────> |
  |                            |                      |
  | <── 16. Forward ─────────  | <── Send(Candidate)  |
  |                            |                      |
  | ...（多個 candidates）       |                      |
  |                            |                      |
  | 17. ICE Complete           |      18. ICE Complete|
  |     (P2P connected)        |                      |
  |                            |                      |
```

### 6.2 時序要求

| 步驟 | 最大延遲 | 超時處理 |
|------|---------|---------|
| ConnectStream | 5s | 重試 |
| Send Offer | 2s | 重試 |
| Receive Answer | 10s | 失敗，嘗試 relay |
| ICE Complete | 30s | 使用 relay 候選 |

---

## 7. 錯誤處理規格

### 7.1 連接錯誤

| 錯誤 | 場景 | 處理 |
|------|------|------|
| `Unavailable` | Signal server 不可達 | 重試連接，指數退避 |
| `Unauthenticated` | 未認證 | 先完成 Management Login |
| `DeadlineExceeded` | Timeout | 重試或使用 relay |

### 7.2 ICE 錯誤

| 錯誤 | 場景 | 處理 |
|------|------|------|
| No candidates | 網絡問題 | 檢查網絡配置 |
| All failed | NAT 太嚴格 | 強制使用 TURN relay |
| Timeout | Peer 離線 | 標記 peer 為 offline |

### 7.3 Fallback 策略

```
1. 嘗試直連（host candidates）
    ↓ 失敗
2. 嘗試通過 STUN（server reflexive）
    ↓ 失敗
3. 使用 TURN relay（中繼）
    ↓ 仍失敗
4. 標記 peer 為不可達
```

**規格**: 必須支持 TURN fallback，確保在任何 NAT 環境下都能連接。

---

## 8. 性能規格

### 8.1 延遲要求

| 操作 | 目標 | 最大 |
|------|------|------|
| ConnectStream | < 200ms | 5s |
| Send Message | < 50ms | 2s |
| ICE Complete | < 5s | 30s |

### 8.2 併發要求

- **Peers**: 支持同時與 100+ peers 建立信令
- **Messages/sec**: > 1000 messages/sec
- **Connections**: 1 long-lived stream per client

### 8.3 資源使用

- **Goroutines**: 2 per stream (send + receive)
- **Memory**: < 10MB per 100 peers
- **Network**: < 5KB/s per peer (平均)

---

## 9. 安全規格

### 9.1 加密要求

**規格**:
- ✅ 所有消息通過 NaCl box 加密
- ✅ 端到端加密（Signal server 無法解密）
- ✅ 每個消息使用隨機 nonce（防重放）
- ✅ 只有目標 peer 能解密消息

### 9.2 認證要求

**規格**:
- ✅ 基於 WireGuard 公鑰認證
- ✅ Signal server 驗證發送者身份
- ✅ 接收者驗證消息來源

### 9.3 防止攻擊

| 攻擊類型 | 防護措施 |
|---------|---------|
| 中間人攻擊 | 端到端加密（基於 WG 密鑰） |
| 重放攻擊 | 隨機 nonce |
| DoS 攻擊 | Rate limiting（server 端） |
| 欺騙攻擊 | 公鑰驗證 |

---

## 10. 實現檢查清單

### 10.1 Signal Client (必須)

- [ ] Signal gRPC client 實現
- [ ] ConnectStream() 實現
- [ ] Send/Receive 消息處理
- [ ] 加密/解密（復用 Management 的 crypto）
- [ ] Goroutine 管理

### 10.2 消息處理 (必須)

- [ ] Offer 處理
- [ ] Answer 處理
- [ ] Candidate 處理
- [ ] 消息路由（按 remotePubKey）

### 10.3 錯誤處理 (必須)

- [ ] 連接重試
- [ ] 超時處理
- [ ] Graceful shutdown

### 10.4 測試 (必須)

- [ ] 單元測試（消息加密/解密）
- [ ] 集成測試（與 Signal server）
- [ ] 端到端測試（兩個 peers）

---

## 11. 數據結構規格

### 11.1 SignalClient

```go
type SignalClient struct {
    url          string
    conn         *grpc.ClientConn
    client       SignalExchangeClient
    stream       SignalExchange_ConnectStreamClient
    ourPrivKey   string
    ourPubKey    string

    // Message channels
    incomingMsgs chan *EncryptedMessage
    outgoingMsgs chan *EncryptedMessage

    // Peer tracking
    activePeers  map[string]bool // WG pubkey -> active status

    // Shutdown
    ctx          context.Context
    cancel       context.CancelFunc
    wg           sync.WaitGroup
}
```

### 11.2 消息 Channel 規格

```go
// Incoming: buffer 100 messages
incomingMsgs := make(chan *EncryptedMessage, 100)

// Outgoing: buffer 100 messages
outgoingMsgs := make(chan *EncryptedMessage, 100)
```

**規格**:
- Buffer size: 100 消息
- Blocking: Send blocks when full
- Close: Graceful shutdown on context cancel

---

## 12. 集成規格

### 12.1 與 Management 集成

```go
// After Management Login
loginResp, _ := mgmtClient.Login(setupKey)

// Get Signal URL from config
signalURL := loginResp.NetbirdConfig.Signal.URI

// Create Signal client
signalClient := NewSignalClient(signalURL, ourPrivKey)
signalClient.Connect()
```

**規格**:
- Signal URL 從 Management 的 `NetbirdConfig` 獲取
- 使用相同的 WireGuard 密鑰對

### 12.2 與 ICE 集成（下一步）

```go
// When Sync receives new peer
func onNewPeer(peer *RemotePeerConfig) {
    // 1. Create ICE agent (Pion)
    agent := ice.NewAgent(...)

    // 2. Generate offer
    offer := agent.CreateOffer()

    // 3. Send via Signal
    signalClient.SendOffer(offer, peer.WgPubKey)

    // 4. Wait for answer
    answer := <-waitForAnswer(peer.WgPubKey)

    // 5. Set remote description
    agent.SetRemoteDescription(answer)

    // 6. Exchange candidates
    // ...

    // 7. Once connected, get endpoint
    selectedPair := agent.GetSelectedCandidatePair()
    endpoint := selectedPair.Remote.Address()

    // 8. Update peers.json
    updatePeerEndpoint(peer.WgPubKey, endpoint)
}
```

**規格**: Signal client 負責信令，ICE agent 負責連接建立。

---

## 13. 測試驗證規格

### 13.1 單元測試

**消息加密測試**:
```go
func TestSignalEncryption(t *testing.T) {
    alicePriv, alicePub := generateKeyPair()
    bobPriv, bobPub := generateKeyPair()

    msg := []byte("test signal message")

    // Alice encrypts for Bob
    encrypted := EncryptMessage(msg, alicePriv, bobPub)

    // Bob decrypts from Alice
    decrypted := DecryptMessage(encrypted, bobPriv, alicePub)

    assert.Equal(t, msg, decrypted)
}
```

### 13.2 集成測試

**ConnectStream 測試**:
```go
func TestConnectStream(t *testing.T) {
    client := NewSignalClient(testSignalURL, testPrivKey)

    err := client.Connect()
    assert.NoError(t, err)

    err = client.ConnectStream(context.Background())
    assert.NoError(t, err)

    // Verify can send/receive
    testMsg := createTestMessage()
    client.SendMessage(testMsg)

    received := <-client.incomingMsgs
    assert.NotNil(t, received)
}
```

### 13.3 端到端測試

**兩個 Peers 測試**:
```bash
# Terminal 1: Peer Alice
./netbird-helper --config-dir /tmp/alice

# Terminal 2: Peer Bob
./netbird-helper --config-dir /tmp/bob

# Verify:
- [ ] Alice 和 Bob 都連接到 Signal server
- [ ] Alice 發送 offer 給 Bob
- [ ] Bob 收到 offer 並回復 answer
- [ ] 雙方交換 candidates
- [ ] ICE 完成，建立 P2P 連接
```

---

## 14. 依賴項

### 14.1 Go 依賴

```go
require (
    golang.org/x/crypto v0.17.0        // NaCl box
    google.golang.org/grpc v1.60.0     // gRPC client
    google.golang.org/protobuf v1.31.0 // Protocol Buffers
    // ICE: 在下一個規格文檔定義
)
```

### 14.2 Proto 文件

- `signalexchange.proto` - Signal 協議定義
- 位置: `/project/netbird/go/proto/signalexchange.proto`

---

## 15. 已知限制

1. **單個 Signal Server**: 不支持多個 Signal server 負載均衡
2. **無優先級**: 所有消息平等處理
3. **無持久化**: 消息不持久化（peer 離線時丟失）
4. **無壓縮**: 消息不壓縮

---

## 16. 未來擴展

1. **多 Signal Server**: 支持故障切換
2. **消息優先級**: Offer/Answer 高優先級
3. **離線消息**: 短期消息緩存
4. **壓縮**: 大消息壓縮

---

## 17. 參考資料

- **Proto 定義**: `/project/netbird/go/proto/signalexchange.proto`
- **ICE RFC**: RFC 8445 - Interactive Connectivity Establishment
- **STUN RFC**: RFC 5389 - Session Traversal Utilities for NAT
- **TURN RFC**: RFC 5766 - Traversal Using Relays around NAT
- **WebRTC**: https://webrtc.org/ (類似概念)

---

**規格版本歷史**:
- v1.0 (2025-12-01): 初始版本（Phase 4C 實現前）

**下一步**: 創建 `ICE_INTEGRATION_SPEC.md` 定義 ICE 集成規格。
