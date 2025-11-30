# NetBird Minimal Go Client - Linux Only

這個目錄包含從 NetBird 官方專案複製的必要 Go 程式碼，專注於 Linux client 的 minimal 實作。

## 目錄結構

```
go/
├── cmd/                    # CLI 命令
│   ├── up.go              # netbird up 命令
│   ├── down.go            # netbird down 命令
│   └── status.go          # netbird status 命令
│
├── internal/              # 核心引擎
│   ├── engine.go          # Engine 主要邏輯（狀態機）
│   ├── connect.go         # 連線管理
│   └── routemanager/      # 路由管理
│       ├── manager.go
│       └── systemops/     # Linux 系統操作
│           └── systemops_linux.go
│
├── iface/                 # WireGuard 介面管理
│   ├── iface.go           # WGIface 介面定義
│   ├── iface_new_linux.go # Linux 實作
│   ├── iface_destroy_linux.go
│   ├── device/            # WireGuard 裝置管理
│   │   ├── wg_link_linux.go
│   │   └── kernel_module_linux.go
│   ├── configurer/        # WireGuard 設定工具
│   │   ├── kernel_unix.go
│   │   └── usp.go
│   └── wgaddr/            # WireGuard 位址處理
│       └── address.go
│
├── management/            # Management Server 客戶端
│   └── client/
│       ├── client.go      # 客戶端介面
│       └── grpc.go        # gRPC 實作
│
├── signal/                # Signal Server 客戶端
│   └── client/
│       ├── client.go      # 客戶端介面
│       ├── grpc.go        # gRPC 實作
│       └── worker.go      # 工作執行緒
│
└── proto/                 # Protocol Buffers 定義
    ├── management.proto   # Management API
    └── signalexchange.proto  # Signal API

```

## 核心元件說明

### 1. Engine (internal/engine.go)
- **功能**: 狀態機，管理整個 client 的生命週期
- **主要方法**:
  - `NewEngine()` - 建立引擎實例
  - `Start()` - 啟動引擎（註冊、建立隧道、啟動同步）
  - `Stop()` - 停止引擎
- **對照 C 實作**: `nb_engine_t` 結構

### 2. WireGuard Interface (iface/)
- **功能**: 管理 WireGuard 網路介面
- **主要操作**:
  - 建立/刪除 WireGuard 介面
  - 更新 peer 設定
  - 管理 AllowedIPs
- **Linux 特定**: 使用 kernel WireGuard + netlink
- **對照 C 實作**: `wg_iface_t` 結構

### 3. Management Client (management/client/)
- **功能**: 與 Management Server 通訊
- **主要操作**:
  - 用 setup-key 註冊 peer
  - 同步 peer 列表和路由
  - 回報狀態
- **Protocol**: gRPC (management.proto)
- **對照 C 實作**: `mgmt_client_t` 結構

### 4. Signal Client (signal/client/)
- **功能**: 與 Signal Server 通訊，用於 P2P 連線
- **主要操作**:
  - 訂閱訊息
  - 交換 SDP offer/answer
  - 交換 ICE candidates
- **Protocol**: gRPC (signalexchange.proto)
- **對照 C 實作**: `signal_client_t` 結構

### 5. Route Manager (internal/routemanager/)
- **功能**: 管理系統路由表
- **主要操作**:
  - 新增/刪除路由規則
  - 設定 NAT masquerading
- **Linux 實作**: netlink API
- **對照 C 實作**: `route_manager_t` 結構

## Minimal 功能範圍

根據 plan.txt，這個 minimal 版本只實作：

1. ✓ Linux 支援（不含其他平台）
2. ✓ Kernel WireGuard（不自己實作 crypto）
3. ✓ 單一 Management + Signal server
4. ✓ 基本 P2P / Relay
5. ✓ 核心流程：setup-key → join network → 建立 tunnel → 互 ping

## 省略的功能

為了簡化 C 移植，以下功能暫不實作：

- SSH 功能 (client/ssh/)
- DNS 管理 (client/internal/dns/)
- Firewall 規則 (client/firewall/)
- UI 相關 (client/ui/)
- Android/iOS/Windows 特定程式碼
- 多網路支援
- Rosenpass (post-quantum)
- eBPF 功能

## 下一步：C 語言移植

這些 Go 檔案將作為參考，移植到 C 語言實作。移植策略：

1. **使用 Linux 現有 library**:
   - WireGuard: `wg` CLI 工具 或 libmnl (netlink)
   - Routes: `ip route` 或 netlink
   - gRPC: protobuf-c + grpc-c
   - ICE: libnice

2. **對應關係**:
   ```
   Go Package              → C Module
   ────────────────────────────────────
   internal/engine.go     → c/engine.c
   iface/                 → c/wg_iface.c
   management/client/     → c/mgmt_client.c
   signal/client/         → c/signal_client.c
   routemanager/          → c/route.c
   cmd/                   → c/nbc_cli.c
   ```

3. **Proto 編譯**:
   ```bash
   protoc --c_out=../c/ proto/*.proto
   ```

## 參考資料

- NetBird 官方專案: https://github.com/netbirdio/netbird
- WireGuard 文件: https://www.wireguard.com/
- 原始 plan.txt 在專案根目錄
