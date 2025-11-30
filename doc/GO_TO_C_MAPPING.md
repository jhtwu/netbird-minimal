# Go 程式碼結構詳解

本文件詳細說明每個 Go 檔案的用途，以及如何對應到 C 實作。

## 檔案清單與說明

### CMD - 命令列介面

#### cmd/up.go
- **功能**: 實作 `netbird up` 命令
- **主要流程**:
  1. 解析命令列參數（setup-key, management-url 等）
  2. 載入或建立設定檔
  3. 建立並啟動 Engine
  4. 等待連線完成
- **C 對應**: `c/nbc_cli.c` 中的 `cmd_up()` 函式

#### cmd/down.go
- **功能**: 實作 `netbird down` 命令
- **主要流程**:
  1. 連接到執行中的 daemon
  2. 發送停止訊號
  3. 清理資源（關閉隧道、刪除路由）
- **C 對應**: `c/nbc_cli.c` 中的 `cmd_down()` 函式

#### cmd/status.go
- **功能**: 實作 `netbird status` 命令
- **主要流程**:
  1. 查詢 daemon 狀態
  2. 顯示連線資訊（peers, IP, 伺服器狀態）
  3. 輸出格式化的狀態報告
- **C 對應**: `c/nbc_cli.c` 中的 `cmd_status()` 函式

---

### Internal - 核心引擎

#### internal/engine.go
- **功能**: Engine 狀態機，整個 client 的核心
- **主要結構**:
  ```go
  type Engine struct {
      ctx           context.Context
      cancel        context.CancelFunc
      config        *Config
      wgIface       iface.WGIface
      routeManager  *routemanager.Manager
      mgmtClient    management.Client
      signalClient  signal.Client
      peers         map[string]*Peer
      status        EngineStatus
  }
  ```
- **主要方法**:
  - `NewEngine()`: 建立引擎
  - `Start()`: 啟動引擎生命週期
  - `Stop()`: 停止引擎
  - `updatePeers()`: 更新 peer 列表
  - `receiveSignal()`: 處理 signal 訊息
- **C 對應**:
  ```c
  typedef struct nb_engine {
      nb_config_t *config;
      wg_iface_t *wg_iface;
      route_manager_t *route_mgr;
      mgmt_client_t *mgmt;
      signal_client_t *signal;
      // peer 列表
      int running;
  } nb_engine_t;

  nb_engine_t* nb_engine_new(nb_config_t *cfg);
  int nb_engine_start(nb_engine_t *eng);
  void nb_engine_stop(nb_engine_t *eng);
  ```

#### internal/connect.go
- **功能**: 管理與 Management Server 的連線和同步
- **主要功能**:
  - 週期性同步 peer 狀態
  - 處理 management 伺服器的更新
  - 重連邏輯
- **C 對應**: 整合到 `c/engine.c` 的 sync loop

#### internal/routemanager/manager.go
- **功能**: 路由管理器介面和邏輯
- **主要方法**:
  - `AddRoute()`: 新增路由
  - `RemoveRoute()`: 移除路由
  - `UpdateRoutes()`: 批次更新路由
- **C 對應**: `c/route.c`

#### internal/routemanager/systemops/systemops_linux.go
- **功能**: Linux 系統路由操作
- **實作方式**:
  - 使用 netlink 或 `ip route` 命令
  - 管理路由表
  - 設定 iptables NAT 規則
- **C 對應**: `c/route.c` 使用 libmnl 或 system() 呼叫

---

### Iface - WireGuard 介面管理

#### iface/iface.go
- **功能**: WGIface 介面定義
- **主要介面**:
  ```go
  type WGIface interface {
      Create() error
      Up() error
      Down() error
      UpdatePeer(pubKey, allowedIPs, endpoint string, keepAlive int) error
      RemovePeer(pubKey string) error
      Close() error
      GetInterfaceName() string
      GetAddress() string
  }
  ```
- **C 對應**:
  ```c
  int wg_iface_create(const nb_config_t *cfg, wg_iface_t **out);
  int wg_iface_up(wg_iface_t *iface);
  int wg_iface_update_peer(wg_iface_t *iface, ...);
  int wg_iface_remove_peer(wg_iface_t *iface, const char *pubkey);
  int wg_iface_close(wg_iface_t *iface);
  ```

#### iface/iface_new_linux.go
- **功能**: Linux 平台建立 WireGuard 介面
- **實作細節**:
  - 偵測 kernel WireGuard vs userspace
  - 建立 netlink 連接
  - 初始化 WireGuard 裝置
- **C 對應**: `c/wg_iface.c` 的 `wg_iface_create()` 實作

#### iface/iface_destroy_linux.go
- **功能**: Linux 平台銷毀 WireGuard 介面
- **實作細節**:
  - 關閉 netlink 連接
  - 刪除網路介面
  - 清理資源
- **C 對應**: `c/wg_iface.c` 的 `wg_iface_close()` 實作

#### iface/device/wg_link_linux.go
- **功能**: Linux WireGuard 裝置的 netlink 操作
- **主要功能**:
  - 使用 netlink 建立 WireGuard 介面
  - 設定介面 IP 位址
  - 啟動/關閉介面
- **使用的 library**: vishvananda/netlink
- **C 對應**: 使用 libmnl 操作 netlink

#### iface/device/kernel_module_linux.go
- **功能**: 檢查 Linux kernel 是否支援 WireGuard
- **檢查方式**:
  - 檢查 `/sys/module/wireguard`
  - 嘗試載入 wireguard 模組
- **C 對應**: 簡單的檔案系統檢查

#### iface/configurer/kernel_unix.go
- **功能**: Unix-like 系統的 WireGuard 設定
- **主要功能**:
  - 使用 `wg` 命令設定 WireGuard
  - 或使用 wgctrl library
  - 管理 private key, listen port, peers
- **C 對應**:
  - Option 1: 使用 `wg` CLI (`system("wg set ...")`)
  - Option 2: 使用 wgctrl C library

#### iface/configurer/usp.go
- **功能**: Userspace WireGuard 設定（wireguard-go）
- **注意**: Minimal 版本使用 kernel WireGuard，可以跳過
- **C 對應**: 不需要

#### iface/wgaddr/address.go
- **功能**: WireGuard 位址解析和驗證
- **主要功能**:
  - 解析 CIDR 格式位址
  - 驗證 IP 位址格式
- **C 對應**: 使用標準 C library (inet_pton, inet_ntop)

---

### Management - Management Server 客戶端

#### management/client/client.go
- **功能**: Management 客戶端介面定義
- **主要介面**:
  ```go
  type Client interface {
      Register(setupKey, pubKey string) (*RegisterResponse, error)
      Sync(peerID string) (*SyncResponse, error)
      IsHealthy() bool
      Close() error
  }
  ```
- **C 對應**:
  ```c
  typedef struct mgmt_client mgmt_client_t;

  mgmt_client_t* mgmt_client_new(const char *url);
  int mgmt_register(mgmt_client_t *c, const char *setup_key, ...);
  int mgmt_sync(mgmt_client_t *c, const char *peer_id, ...);
  void mgmt_client_free(mgmt_client_t *c);
  ```

#### management/client/grpc.go
- **功能**: Management gRPC 客戶端實作
- **主要功能**:
  - 建立 gRPC 連線
  - 實作 RegisterPeer RPC
  - 實作 SyncPeerState RPC
  - 處理重連和錯誤
- **使用 proto**: management.proto
- **C 對應**: 使用 grpc-c 或 protobuf-c

---

### Signal - Signal Server 客戶端

#### signal/client/client.go
- **功能**: Signal 客戶端介面定義
- **主要介面**:
  ```go
  type Client interface {
      Send(msg *Message) error
      Receive(ctx context.Context) (<-chan *Message, error)
      Close() error
  }
  ```
- **C 對應**:
  ```c
  typedef struct signal_client signal_client_t;

  signal_client_t* signal_client_new(const char *url);
  int signal_send(signal_client_t *c, const char *to, const char *msg_type, ...);
  int signal_receive(signal_client_t *c, signal_message_t *msg_out);
  void signal_client_free(signal_client_t *c);
  ```

#### signal/client/grpc.go
- **功能**: Signal gRPC 客戶端實作
- **主要功能**:
  - 建立雙向 gRPC stream
  - 發送 offer/answer/candidate
  - 接收 signal 訊息
- **使用 proto**: signalexchange.proto
- **C 對應**: 使用 grpc-c 的 streaming API

#### signal/client/worker.go
- **功能**: Signal 訊息處理 worker
- **主要功能**:
  - 背景執行緒接收訊息
  - 訊息佇列管理
  - 重連處理
- **C 對應**: 使用 pthread 實作 worker thread

---

### Proto - Protocol Buffers 定義

#### proto/management.proto
- **功能**: Management Server API 定義
- **主要 Service**:
  ```protobuf
  service ManagementService {
      rpc RegisterPeer(RegisterPeerRequest) returns (RegisterPeerResponse);
      rpc SyncPeerState(SyncRequest) returns (SyncResponse);
      rpc SendPeerStatus(PeerStatus) returns (Empty);
  }
  ```
- **主要 Message**:
  - `RegisterPeerRequest`: setup key, peer public key
  - `RegisterPeerResponse`: peer ID, peer IP, peer list, routes
  - `SyncRequest`: peer ID
  - `SyncResponse`: updated peer list, routes
  - `PeerInfo`: peer 詳細資訊
  - `Route`: 路由規則
- **C 對應**: 使用 protoc-c 生成 C 程式碼
  ```bash
  protoc --c_out=../c/ management.proto
  ```

#### proto/signalexchange.proto
- **功能**: Signal Server API 定義
- **主要 Service**:
  ```protobuf
  service SignalExchange {
      rpc Send(Message) returns (Empty);
      rpc Receive(ReceiveRequest) returns (stream Message);
  }
  ```
- **主要 Message**:
  - `Message`: from, to, body
  - `Body`: offer | answer | ice_candidate
  - `Offer`: SDP offer
  - `Answer`: SDP answer
  - `IceCandidate`: ICE candidate string
- **C 對應**: 使用 protoc-c 生成 C 程式碼
  ```bash
  protoc --c_out=../c/ signalexchange.proto
  ```

---

## 依賴關係圖

```
cmd/
  ├─> internal/engine.go
  │     ├─> iface/
  │     │     └─> device/
  │     │     └─> configurer/
  │     ├─> management/client/
  │     │     └─> proto/management.proto
  │     ├─> signal/client/
  │     │     └─> proto/signalexchange.proto
  │     └─> routemanager/
  │           └─> systemops/
  └─> config (未包含，需補充)
```

## C 移植優先順序

根據 plan.txt 的建議順序：

### Phase 1: WireGuard + Route
1. `iface/device/wg_link_linux.go` → `c/wg_iface.c`
2. `iface/configurer/kernel_unix.go` → `c/wg_iface.c`
3. `routemanager/systemops/systemops_linux.go` → `c/route.c`

**測試目標**: 手動建立 WG 介面，連接到現有 WireGuard server

### Phase 2: Management Client
1. `proto/management.proto` → `c/management.pb-c.h/c`
2. `management/client/grpc.go` → `c/mgmt_client.c`

**測試目標**: 用 setup-key 註冊，在 Management UI 看到 peer

### Phase 3: Engine 整合
1. `internal/engine.go` → `c/engine.c`
2. 整合 Phase 1 + Phase 2

**測試目標**: 兩台機器在 relay 模式互 ping

### Phase 4: Signal Client + ICE
1. `proto/signalexchange.proto` → `c/signal.pb-c.h/c`
2. `signal/client/grpc.go` → `c/signal_client.c`
3. 整合 libnice (ICE library)

**測試目標**: P2P 模式互 ping

### Phase 5: CLI
1. `cmd/up.go` → `c/nbc_cli.c`
2. `cmd/down.go` → `c/nbc_cli.c`
3. `cmd/status.go` → `c/nbc_cli.c`

**測試目標**: 完整 CLI 工作流程

---

## 重要的 Go 特性與 C 對應

### 1. Context (ctx context.Context)
- **Go**: 用於取消和超時控制
- **C**: 使用 signal handler + flag, 或 pthread_cancel

### 2. Goroutines
- **Go**: `go func() { ... }()`
- **C**: `pthread_create()`

### 3. Channels
- **Go**: `ch := make(chan Message)`
- **C**: 使用 pthread condition variables 或 pipe

### 4. Defer
- **Go**: `defer cleanup()`
- **C**: 手動在每個返回路徑呼叫 cleanup，或使用 goto

### 5. Error Handling
- **Go**: `if err != nil { return err }`
- **C**: `if (ret < 0) { return -1; }`

### 6. Interfaces
- **Go**: `type WGIface interface { ... }`
- **C**: Function pointers 或直接實作

---

## 省略的檔案

以下檔案在 minimal 版本中不需要：

- `*_test.go`: 測試檔案
- `*_windows.go`, `*_darwin.go`, `*_android.go`: 其他平台
- `dns/`, `firewall/`, `ssh/`: 額外功能
- `usp.go`: Userspace WireGuard（我們用 kernel）
- `ui/`, `wasm/`: UI 相關

---

## 額外需要的檔案

以下檔案需要從官方專案補充或自行建立：

1. **Config 管理**:
   - 需要一個 config.go 處理 JSON 設定檔
   - C 版本使用 cJSON library

2. **Crypto**:
   - WireGuard key 生成和管理
   - C 版本使用 libsodium 或 WireGuard tools

3. **Logger**:
   - 統一的 logging 機制
   - C 版本使用 syslog 或簡單的 fprintf

---

## 參考資料

- NetBird 官方: https://github.com/netbirdio/netbird
- WireGuard: https://www.wireguard.com/
- netlink: https://man7.org/linux/man-pages/man7/netlink.7.html
- gRPC C: https://github.com/grpc/grpc/tree/master/src/core
- protobuf-c: https://github.com/protobuf-c/protobuf-c
- libnice (ICE): https://nice.freedesktop.org/
