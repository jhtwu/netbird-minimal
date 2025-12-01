# ICE Integration Specification

**版本**: 1.0
**日期**: 2025-12-01
**狀態**: Specification (Phase 4C - Not Yet Implemented)
**作者**: NetBird Minimal C Client Team

## 1. ICE 概述

### 1.1 目的
ICE (Interactive Connectivity Establishment) 用於：
- NAT 穿透（在防火牆後建立 P2P 連接）
- 自動發現 peer 的真實網絡地址
- 選擇最佳連接路徑（直連 > STUN > TURN）
- 處理對稱 NAT 等複雜網絡環境

### 1.2 NetBird 中的 ICE

NetBird 使用 ICE 來：
1. 發現 peer 的 endpoint（IP:Port）
2. 建立 WireGuard tunnel 的連接路徑
3. 更新 `peers.json` 中的 `endpoint` 字段
4. 動態適應網絡變化

### 1.3 實現庫

**選擇**: **Pion ICE** (https://github.com/pion/ice)

**理由**:
- ✅ 純 Go 實現，無 C 依賴
- ✅ 完整的 ICE RFC 8445 實現
- ✅ 支持 STUN 和 TURN
- ✅ 活躍維護，文檔完善
- ✅ NetBird 官方也使用 Pion

---

## 2. ICE 工作流程

### 2.1 完整流程

```
1. Management Sync
   ↓ 收到新 peer
2. 創建 ICE Agent
   ↓
3. 收集 Candidates
   - Host (本地 IP)
   - Server Reflexive (STUN)
   - Relay (TURN)
   ↓
4. 通過 Signal 交換 Candidates
   ↓
5. ICE 連接檢查
   - Connectivity checks
   - Candidate pairing
   ↓
6. 選擇最佳 Candidate Pair
   ↓
7. 獲取 Remote Endpoint
   ↓
8. 更新 peers.json
   ↓
9. C Client 讀取並配置 WireGuard
```

### 2.2 時序圖

```
Management    Signal    ICE Agent    STUN Server    Peer
    |            |           |            |           |
    | Peer List  |           |            |           |
    |─────────> |           |            |           |
    |            |           |            |           |
    |        Create ICE Agent             |           |
    |            |───────────>            |           |
    |            |           |            |           |
    |            |      Gather Candidates |           |
    |            |           |──────────> |           |
    |            |           | <────────  |           |
    |            |           | (STUN resp)|           |
    |            |           |            |           |
    |            | Send Offer/Candidates  |           |
    |            |<──────────|            |           |
    |            |──────────────────────────────────> |
    |            |                        |           |
    |            | <────────────────────────────────  |
    |            | Receive Answer/Candidates          |
    |            |──────────>            |           |
    |            |           |            |           |
    |            |      Connectivity Check           |
    |            |           |──────────────────────> |
    |            |           | <────────────────────  |
    |            |           | (STUN binding)         |
    |            |           |            |           |
    |            |      Select Best Pair  |           |
    |            |           |            |           |
    |            |      Get Endpoint      |           |
    |            |           |            |           |
    |         Update peers.json           |           |
```

---

## 3. ICE Agent 規格

### 3.1 創建 Agent

```go
import (
    "github.com/pion/ice/v2"
    "github.com/pion/stun"
)

type ICEAgent struct {
    agent         *ice.Agent
    peerPubKey    string
    localUfrag    string
    localPwd      string
    remoteUfrag   string
    remotePwd     string
    onCandidate   func(ice.Candidate)
    onComplete    func(string) // endpoint
}

func NewICEAgent(peerPubKey string, stunURLs []string) (*ICEAgent, error) {
    // 1. 配置
    config := &ice.AgentConfig{
        NetworkTypes: []ice.NetworkType{
            ice.NetworkTypeUDP4,
            ice.NetworkTypeUDP6,
        },
        Urls: stunURLs,  // STUN servers
        // TURN servers 在需要時添加
    }

    // 2. 創建 Agent
    agent, err := ice.NewAgent(config)
    if err != nil {
        return nil, err
    }

    // 3. 生成認證憑據
    ufrag, pwd := ice.GenerateAuthCredentials()

    return &ICEAgent{
        agent:      agent,
        peerPubKey: peerPubKey,
        localUfrag: ufrag,
        localPwd:   pwd,
    }, nil
}
```

**規格**:
- **Network Types**: 支持 IPv4 和 IPv6
- **STUN URLs**: 從 Management 的 `NetbirdConfig.Stuns` 獲取
- **Credentials**: 每個 peer 連接使用唯一的 ufrag/pwd

### 3.2 Candidate 類型

| Type | 優先級 | 說明 | 示例 |
|------|-------|------|------|
| Host | 高 | 本地網絡接口 | 192.168.1.100:51820 |
| Server Reflexive | 中 | 通過 STUN 發現的公網地址 | 203.0.113.50:12345 |
| Relay | 低 | 通過 TURN 中繼 | relay.netbird.io:3478 |

**規格**: 優先使用 Host，其次 Server Reflexive，最後 Relay。

---

## 4. Candidate 收集規格

### 4.1 收集流程

```go
func (a *ICEAgent) GatherCandidates() error {
    // 1. 設置 candidate 回調
    a.agent.OnCandidate(func(c ice.Candidate) {
        if c == nil {
            // Gathering complete
            return
        }

        // 2. 發送 candidate 到 Signal
        a.onCandidate(c)
    })

    // 3. 開始收集
    err := a.agent.GatherCandidates()
    if err != nil {
        return err
    }

    return nil
}
```

**規格**:
- **Timeout**: 5 秒內完成收集
- **最少 Candidates**: 至少 1 個（host candidate）
- **回調**: 每個 candidate 發現後立即通過 Signal 發送

### 4.2 Candidate 格式

**SDP 格式**（用於 Signal 傳輸）:
```
candidate:1 1 UDP 2130706431 192.168.1.100 51820 typ host
candidate:2 1 UDP 1694498815 203.0.113.50 12345 typ srflx raddr 192.168.1.100 rport 51820
candidate:3 1 UDP 16777215 relay.netbird.io 3478 typ relay raddr 203.0.113.50 rport 12345
```

**結構**:
- **Foundation**: 唯一標識符
- **Component**: 1 (RTP)
- **Protocol**: UDP
- **Priority**: 優先級數值
- **IP**: 地址
- **Port**: 端口
- **Type**: host/srflx/relay

---

## 5. ICE 連接建立規格

### 5.1 Offer/Answer 交換

```go
// Controlling peer (Offer)
func (a *ICEAgent) CreateOffer() (string, string, error) {
    return a.localUfrag, a.localPwd, nil
}

// Controlled peer (Answer)
func (a *ICEAgent) CreateAnswer(remoteUfrag, remotePwd string) error {
    a.remoteUfrag = remoteUfrag
    a.remotePwd = remotePwd
    return nil
}
```

**規格**:
- **Controlling**: Peer with higher priority creates offer
- **Controlled**: Other peer creates answer
- **Priority**: 比較 WireGuard 公鑰字符串（字典序）

**優先級判斷**:
```go
func isControlling(ourPubKey, peerPubKey string) bool {
    return ourPubKey > peerPubKey  // 字典序比較
}
```

### 5.2 連接檢查

```go
func (a *ICEAgent) StartConnectivityChecks() error {
    // 1. 設置遠程認證
    err := a.agent.SetRemoteCredentials(a.remoteUfrag, a.remotePwd)
    if err != nil {
        return err
    }

    // 2. 設置 state change 回調
    a.agent.OnConnectionStateChange(func(state ice.ConnectionState) {
        switch state {
        case ice.ConnectionStateConnected:
            // 連接成功
            a.onConnected()
        case ice.ConnectionStateFailed:
            // 連接失敗
            a.onFailed()
        case ice.ConnectionStateCompleted:
            // ICE complete
            a.onComplete()
        }
    })

    return nil
}

func (a *ICEAgent) onConnected() {
    // 獲取選中的 candidate pair
    pair := a.agent.GetSelectedCandidatePair()
    if pair == nil {
        return
    }

    // 提取 remote endpoint
    endpoint := fmt.Sprintf("%s:%d",
        pair.Remote.Address(),
        pair.Remote.Port())

    // 回調通知
    if a.onComplete != nil {
        a.onComplete(endpoint)
    }
}
```

**規格**:
- **Timeout**: 30 秒
- **Retries**: STUN binding requests 重試 7 次
- **Interval**: 每 50ms 發送一次 check

---

## 6. STUN 配置規格

### 6.1 STUN Server 列表

從 Management 獲取:
```json
{
  "stuns": [
    {
      "uri": "stun:stun.netbird.io:3478",
      "protocol": "UDP"
    },
    {
      "uri": "stun:stun.l.google.com:19302",
      "protocol": "UDP"
    }
  ]
}
```

**規格**:
- **Format**: `stun:host:port`
- **Protocol**: UDP (預設)
- **Timeout**: 3 秒
- **Retries**: 3 次

### 6.2 STUN 使用

```go
func parseSTUNURLs(config *NetbirdConfig) []string {
    urls := make([]string, 0)
    for _, stun := range config.Stuns {
        urls = append(urls, stun.URI)
    }
    return urls
}

// 創建 Agent 時使用
agent, err := NewICEAgent(peerPubKey, parseSTUNURLs(config))
```

---

## 7. TURN 配置規格（可選）

### 7.1 TURN Server 列表

從 Management 獲取:
```json
{
  "turns": [
    {
      "hostConfig": {
        "uri": "turn:turn.netbird.io:3478",
        "protocol": "UDP"
      },
      "user": "username",
      "password": "credential"
    }
  ]
}
```

### 7.2 TURN 使用

```go
func configureTURN(config *NetbirdConfig) []ice.URL {
    urls := make([]ice.URL, 0)

    for _, turn := range config.Turns {
        url := ice.URL{
            Scheme:   ice.SchemeTypeTURN,
            Host:     turn.HostConfig.URI,
            Username: turn.User,
            Password: turn.Password,
            Proto:    ice.ProtoTypeUDP,
        }
        urls = append(urls, url)
    }

    return urls
}
```

**規格**:
- **何時使用**: 當直連和 STUN 都失敗時
- **Cost**: TURN 有帶寬成本，最後使用
- **Timeout**: 10 秒

---

## 8. Endpoint 更新規格

### 8.1 更新 peers.json

```go
func (h *Helper) updatePeerEndpoint(peerPubKey, endpoint string) error {
    // 1. 讀取現有 peers.json
    peers, err := h.loadPeers()
    if err != nil {
        return err
    }

    // 2. 找到對應 peer
    for i, peer := range peers.Peers {
        if peer.PublicKey == peerPubKey {
            // 3. 更新 endpoint
            peers.Peers[i].Endpoint = endpoint
            log.Printf("Updated peer %s endpoint: %s",
                peerPubKey[:16]+"...", endpoint)
            break
        }
    }

    // 4. 寫回 peers.json（atomic）
    peers.UpdatedAt = time.Now().Format(time.RFC3339)
    return h.writePeers(peers)
}
```

**規格**:
- **Atomic Write**: 使用 temp file + rename
- **Timestamp**: 更新 `updatedAt` 字段
- **Notification**: 寫入後觸發 C client 的 inotify

### 8.2 更新後的 peers.json

```json
{
  "peers": [
    {
      "id": "6TqWclrW1bRKvI340AkPnEJ1aeYp8HSCYt9WiYS7ljs=",
      "publicKey": "6TqWclrW1bRKvI340AkPnEJ1aeYp8HSCYt9WiYS7ljs=",
      "endpoint": "203.0.113.50:51820",  // ✅ 已更新！
      "allowedIPs": ["100.72.243.30/32"],
      "keepalive": 25
    }
  ],
  "updatedAt": "2025-12-01T14:30:00+08:00"
}
```

---

## 9. ICE 狀態機規格

### 9.1 狀態轉換

```
New
  ↓ GatherCandidates()
Gathering
  ↓ OnCandidate(nil)
GatheringComplete
  ↓ SetRemoteCredentials()
Checking
  ↓ Connectivity checks
Connected
  ↓ All checks done
Completed
```

### 9.2 錯誤狀態

```
Checking
  ↓ All checks failed
Failed
  ↓ Retry or use TURN
```

**規格**: Failed 狀態時，嘗試添加 TURN relay candidates 並重試。

---

## 10. 併發管理規格

### 10.1 每個 Peer 一個 Goroutine

```go
type ICEManager struct {
    agents map[string]*ICEAgent  // peerPubKey -> agent
    mu     sync.RWMutex
}

func (m *ICEManager) StartICEForPeer(peer *RemotePeerConfig) {
    m.mu.Lock()
    defer m.mu.Unlock()

    // Check if already processing
    if _, exists := m.agents[peer.WgPubKey]; exists {
        return
    }

    // Create agent
    agent := NewICEAgent(peer.WgPubKey, m.stunURLs)

    // Store
    m.agents[peer.WgPubKey] = agent

    // Start in goroutine
    go m.runICEForPeer(agent, peer)
}

func (m *ICEManager) runICEForPeer(agent *ICEAgent, peer *RemotePeerConfig) {
    defer func() {
        m.mu.Lock()
        delete(m.agents, peer.WgPubKey)
        m.mu.Unlock()
    }()

    // 1. Gather candidates
    agent.GatherCandidates()

    // 2. Exchange via Signal
    // ...

    // 3. Start connectivity checks
    agent.StartConnectivityChecks()

    // 4. Wait for completion
    // ...
}
```

**規格**:
- **Concurrency**: 每個 peer 獨立 goroutine
- **Max Concurrent**: 限制同時處理的 peers（如 50）
- **Cleanup**: Goroutine 結束後清理資源

---

## 11. 性能規格

### 11.1 延遲要求

| 操作 | 目標 | 最大 |
|------|------|------|
| Create Agent | < 10ms | 100ms |
| Gather Candidates | < 1s | 5s |
| Connectivity Checks | < 3s | 30s |
| Total (New Peer → Endpoint) | < 5s | 30s |

### 11.2 資源使用

| 資源 | 單個 Peer | 100 Peers |
|------|----------|-----------|
| Memory | ~1MB | ~100MB |
| Goroutines | 2 | 200 |
| Network (STUN) | < 1KB | < 100KB |

---

## 12. 錯誤處理規格

### 12.1 常見錯誤

| 錯誤 | 場景 | 處理 |
|------|------|------|
| No candidates | 網絡不可達 | 報錯，不嘗試連接 |
| STUN timeout | STUN server 不可達 | 使用 host candidate |
| All checks failed | NAT 太嚴格 | 嘗試 TURN |
| TURN failed | 無可用中繼 | 標記 peer 不可達 |

### 12.2 重試策略

```go
func (m *ICEManager) retryICE(peer *RemotePeerConfig) {
    for attempt := 1; attempt <= 3; attempt++ {
        err := m.attemptICE(peer)
        if err == nil {
            return
        }

        log.Printf("ICE attempt %d failed: %v", attempt, err)

        if attempt < 3 {
            // Exponential backoff
            time.Sleep(time.Duration(1<<attempt) * time.Second)
        }
    }

    // After 3 attempts, use TURN
    m.attemptICEWithTURN(peer)
}
```

**規格**:
- **Retries**: 3 次
- **Backoff**: 2s, 4s, 8s
- **Fallback**: TURN relay

---

## 13. 測試驗證規格

### 13.1 單元測試

**Candidate 收集測試**:
```go
func TestGatherCandidates(t *testing.T) {
    agent := NewICEAgent("test-peer", []string{"stun:stun.l.google.com:19302"})

    candidates := []ice.Candidate{}
    agent.OnCandidate(func(c ice.Candidate) {
        if c != nil {
            candidates = append(candidates, c)
        }
    })

    err := agent.GatherCandidates()
    assert.NoError(t, err)

    // Wait for gathering
    time.Sleep(2 * time.Second)

    // Should have at least 1 host candidate
    assert.NotEmpty(t, candidates)

    hasHost := false
    for _, c := range candidates {
        if c.Type() == ice.CandidateTypeHost {
            hasHost = true
            break
        }
    }
    assert.True(t, hasHost)
}
```

### 13.2 集成測試

**兩個 Peers ICE 測試**:
```go
func TestICEConnectivity(t *testing.T) {
    // Create two agents
    alice := NewICEAgent("alice", stunURLs)
    bob := NewICEAgent("bob", stunURLs)

    // Gather candidates
    alice.GatherCandidates()
    bob.GatherCandidates()

    // Exchange credentials
    aliceUfrag, alicePwd := alice.CreateOffer()
    bobUfrag, bobPwd := bob.CreateOffer()

    alice.CreateAnswer(bobUfrag, bobPwd)
    bob.CreateAnswer(aliceUfrag, alicePwd)

    // Exchange candidates (simulate Signal)
    // ...

    // Start connectivity checks
    alice.StartConnectivityChecks()
    bob.StartConnectivityChecks()

    // Wait for connection
    timeout := time.After(10 * time.Second)
    connected := false

    select {
    case <-alice.OnConnected():
        connected = true
    case <-timeout:
        t.Fatal("Connection timeout")
    }

    assert.True(t, connected)
}
```

### 13.3 端到端測試

```bash
# 1. 啟動兩個 helper instances
./netbird-helper --config-dir /tmp/peer1 &
./netbird-helper --config-dir /tmp/peer2 &

# 2. 等待 ICE 完成
sleep 10

# 3. 檢查 peers.json
cat /tmp/peer1/peers.json | jq '.peers[0].endpoint'
# Expected: "203.0.113.50:51820" (非空)

# 4. 驗證 WireGuard 連接
ping -c 3 100.72.49.38  # Peer2 的 VPN IP
# Expected: 成功 ping
```

---

## 14. 實現檢查清單

### 14.1 ICE Agent (必須)

- [ ] Pion ICE 集成
- [ ] Agent 創建和配置
- [ ] Candidate 收集
- [ ] Offer/Answer 處理
- [ ] Connectivity checks

### 14.2 STUN 集成 (必須)

- [ ] 從 Management 獲取 STUN URLs
- [ ] STUN binding requests
- [ ] Server reflexive candidates

### 14.3 TURN 集成 (可選)

- [ ] 從 Management 獲取 TURN 配置
- [ ] TURN allocation
- [ ] Relay candidates

### 14.4 Signal 集成 (必須)

- [ ] 通過 Signal 發送 candidates
- [ ] 通過 Signal 接收 candidates
- [ ] Offer/Answer 交換

### 14.5 Endpoint 更新 (必須)

- [ ] 獲取 selected candidate pair
- [ ] 提取 endpoint
- [ ] 更新 peers.json
- [ ] Atomic write

### 14.6 併發管理 (必須)

- [ ] ICEManager 實現
- [ ] Per-peer goroutines
- [ ] 資源清理
- [ ] 錯誤處理

### 14.7 測試 (必須)

- [ ] 單元測試
- [ ] 集成測試
- [ ] 端到端測試

---

## 15. 依賴項

### 15.1 Go 依賴

```go
require (
    github.com/pion/ice/v2 v2.3.11
    github.com/pion/stun v0.6.1
    github.com/pion/turn/v2 v2.1.4  // 如果使用 TURN
    github.com/pion/logging v0.2.2
)
```

### 15.2 系統要求

- **UDP 端口**: 需要開放 UDP 端口範圍（通常 49152-65535）
- **防火牆**: 允許 UDP 出站連接
- **STUN**: 能訪問公網 STUN server

---

## 16. 已知限制

1. **IPv6**: 初期只支持 IPv4
2. **TCP candidates**: 不支持 TCP candidates
3. **mDNS**: 不支持 mDNS candidates
4. **代理**: 不支持 SOCKS/HTTP 代理

---

## 17. 安全考量

### 17.1 STUN 安全

- ✅ STUN 是無狀態的，不需要認證
- ⚠️ STUN 響應可能被偽造（中間人攻擊）
- ✅ ICE 使用 STUN transaction ID 防止偽造

### 17.2 TURN 安全

- ✅ TURN 需要認證（username/password）
- ✅ 短期憑證（time-limited credentials）
- ✅ 防止中繼濫用

---

## 18. 參考資料

- **Pion ICE**: https://github.com/pion/ice
- **RFC 8445**: Interactive Connectivity Establishment (ICE)
- **RFC 5389**: STUN
- **RFC 5766**: TURN
- **NetBird 源碼**: `/project/netbird/go/` (參考實現)

---

**規格版本歷史**:
- v1.0 (2025-12-01): 初始版本（Phase 4C 實現前）

**下一步**: 根據此規格實現 ICE 集成，並與 Signal Protocol 集成。
