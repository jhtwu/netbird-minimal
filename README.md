# NetBird Client - Go to C Porting Project

NetBird 是一個開源的 WireGuard VPN 平台。本專案目標是將 NetBird client 從 Go 語言移植到 C 語言，專注於 Linux 平台的最小化實作。

## 專案狀態

- ✅ **Phase 1**: Go 程式碼準備完成
- ✅ **Phase 2-3（原型）**: C 端已具備 WireGuard/Route/JSON/CLI，僅支援手動 peers/路由
- ⏭️ **Phase 4-5**: Management/Signal gRPC、setup-key 自動註冊、ICE/P2P、Daemon 模式

> **現況提示**
> - Management/Signal/ICE 尚未實作，暫無自動註冊或同步；需手動新增 peers/路由。
> - CLI 名稱：`netbird-client`，預設介面為 `wtnb0`（避免覆蓋系統已有的 `wt0`）。
> - 設定格式支援 NetBird CamelCase 與 snake_case，建議使用 CamelCase。

## 目錄結構

```
netbird/
├── plan.txt                    # 詳細移植計劃（必讀）
├── GO_PORTING_SUMMARY.md      # Go 階段完成總結
├── README.md                   # 本檔案
│
├── go/                         # Go 原始程式碼參考
│   ├── README.md              # Go 程式碼說明
│   ├── STRUCTURE.md           # 詳細檔案說明與 C 對應
│   ├── go.mod                 # Go 模組定義
│   ├── cmd/                   # CLI 命令
│   ├── internal/              # 核心引擎
│   ├── iface/                 # WireGuard 介面
│   ├── management/            # Management 客戶端
│   ├── signal/                # Signal 客戶端
│   └── proto/                 # Protocol Buffers 定義
│
└── c/                          # C 語言實作（Phase 1~3 原型）
    ├── include/               # 標頭 (config/engine/wg_iface/route)
    ├── src/                   # 實作 (WireGuard、Route、Config、Engine、CLI)
    ├── test/                  # 測試 (wg/route/config/engine/CLI workflow)
    └── build/                 # 編譯輸出
```

## 專案目標

實作一個 **Minimal NetBird C Client**，滿足以下條件：

### 支援範圍
- ✅ **平台**: 僅 Linux（包含 OpenWrt）
- ✅ **WireGuard**: 使用 Kernel WireGuard + 現有 C library
- ✅ **功能**: setup-key → join network → 建立 tunnel → 互 ping
- ✅ **連線**: 基本 P2P / Relay（使用 libnice for ICE）

### 不支援
- ❌ 其他平台（Windows, macOS, Android, iOS）
- ❌ 多網路支援
- ❌ SSH 功能
- ❌ DNS 管理
- ❌ Firewall 規則
- ❌ UI 介面

## 快速開始

### 1. 理解專案
1. 閱讀 `plan.txt` - 了解整體移植計劃
2. 閱讀 `GO_PORTING_SUMMARY.md` - 了解當前進度
3. 閱讀 `go/README.md` - 了解 Go 程式碼結構
4. 閱讀 `go/STRUCTURE.md` - 了解詳細的 Go 到 C 對應

### 2. Go 程式碼參考

**重要**: `go/` 目錄中的程式碼**不是用來編譯執行的**！

這些是從 NetBird 官方專案抽取的參考實作，用於：
- 理解 NetBird client 的運作邏輯
- 作為 C 語言移植的參考
- 查看資料結構和 API 設計

這些 Go 檔案依賴完整的 NetBird 專案，無法單獨編譯。

**如何使用**：

```bash
# 閱讀和理解邏輯（不要嘗試編譯）
cat go/internal/engine.go         # 核心引擎
cat go/iface/iface_new_linux.go   # WireGuard 介面
cat go/management/client/grpc.go  # Management 客戶端

# 查看 Proto 定義
cat go/proto/management.proto
cat go/proto/signalexchange.proto

# 詳細說明
cat go/README_GO_CODE.md
```

**如果需要執行 Go 版本**，請使用官方完整專案：
```bash
git clone https://github.com/netbirdio/netbird /tmp/netbird-full
cd /tmp/netbird-full/client
go build -o netbird
sudo ./netbird up --setup-key YOUR_KEY
```

### 3. C 端現況（Phase 1~3 原型）
- 已有：WireGuard/Route（shell 命令原型）、JSON 設定、Engine、CLI (`netbird-client`)。
- 未有：Management/Signal gRPC、setup-key/自動註冊、ICE/P2P、Daemon/IPC。
- 介面預設：`wtnb0`；測試用 `wtnb-cli0`，避免覆蓋系統既有介面。
- 使用方式：參考 `c/README.md`（包含建置與測試），目前僅支援手動新增 peers/路由。

後續開發順序可參考 `plan.txt`：Phase 4（Signal/ICE）、Phase 5（完整 CLI/Daemon）。

## 技術棧

### Go 版本使用
- pion/ice - ICE/STUN/TURN
- google.golang.org/grpc - gRPC
- vishvananda/netlink - Linux netlink
- golang.zx2c4.com/wireguard/wgctrl - WireGuard 控制

### C 版本將使用
- **WireGuard**: `wg` CLI 工具 或 libmnl (netlink)
- **路由**: `ip route` CLI 或 libmnl (netlink)
- **gRPC**: grpc-c
- **Protocol Buffers**: protobuf-c
- **ICE**: libnice
- **JSON Config**: cJSON
- **Logging**: syslog 或 fprintf

## 實作策略

### 快速原型（建議先做）
- 使用 shell 命令 (`wg`, `ip route`)
- 先實作 relay 模式
- 單執行緒
- **時間**: 1-2 週可完成基本功能

### 正式版本（之後優化）
- 使用 netlink library (libmnl)
- 完整 ICE 實作 (libnice)
- 多執行緒 (pthread)
- 完善錯誤處理

## 核心流程

### Client 啟動流程
```
nbc up --setup-key XXX
  └─> 建立 nb_engine_t
       ├─> 載入/建立 config.json
       ├─> 初始化 mgmt_client
       ├─> 初始化 signal_client
       ├─> 初始化 wg_iface
       ├─> 初始化 route_manager
       │
       ├─> mgmt_register(setup_key)
       │    └─> 收到：peer_id, wg_addr, peers, routes
       │
       ├─> wg_iface_create()
       │    └─> 建立 WireGuard 介面
       │    └─> 設定 IP 和 private key
       │
       ├─> 為每個 peer:
       │    └─> wg_iface_update_peer()
       │
       ├─> 為每個 route:
       │    └─> route_add()
       │
       └─> 主迴圈:
            ├─> signal_receive() (背景執行緒)
            │    └─> 處理 offer/answer
            │    └─> 更新 WG endpoint
            │
            └─> mgmt_sync() (定期)
                 └─> 更新 peers/routes
```

### Client 停止流程
```
nbc down
  └─> nb_engine_stop()
       ├─> signal_client_free()
       ├─> mgmt_client_free()
       ├─> route_remove() (所有路由)
       ├─> wg_iface_remove_peer() (所有 peers)
       └─> wg_iface_destroy()
```

## 預期的 C API

### Engine
```c
typedef struct nb_engine nb_engine_t;

nb_engine_t* nb_engine_new(const nb_config_t *cfg);
int nb_engine_start(nb_engine_t *eng);
void nb_engine_stop(nb_engine_t *eng);
void nb_engine_free(nb_engine_t *eng);
```

### WireGuard Interface
```c
typedef struct wg_iface wg_iface_t;

int wg_iface_create(const nb_config_t *cfg, wg_iface_t **out);
int wg_iface_up(wg_iface_t *iface);
int wg_iface_update_peer(wg_iface_t *iface, const char *pubkey,
                         const char *allowed_ips, int keepalive,
                         const char *endpoint, const char *psk);
int wg_iface_remove_peer(wg_iface_t *iface, const char *pubkey);
int wg_iface_destroy(wg_iface_t *iface);
```

### Management Client
```c
typedef struct mgmt_client mgmt_client_t;

mgmt_client_t* mgmt_client_new(const char *url);
int mgmt_register(mgmt_client_t *c, const char *setup_key,
                 const char *wg_pubkey, /* out params */);
int mgmt_sync(mgmt_client_t *c, const char *peer_id, /* out params */);
void mgmt_client_free(mgmt_client_t *c);
```

### Signal Client
```c
typedef struct signal_client signal_client_t;

signal_client_t* signal_client_new(const char *url);
int signal_subscribe(signal_client_t *c, const char *peer_id);
int signal_send(signal_client_t *c, const char *to_peer,
               const char *msg_type, const char *content);
int signal_receive(signal_client_t *c, signal_message_t *msg_out);
void signal_client_free(signal_client_t *c);
```

### Route Manager
```c
typedef struct route_manager route_manager_t;

route_manager_t* route_manager_new(const char *wg_device);
int route_add(route_manager_t *mgr, const route_config_t *route);
int route_remove(route_manager_t *mgr, const char *network);
void route_manager_free(route_manager_t *mgr);
```

## 參考文件

### 專案內文件
- `plan.txt` - 詳細移植計劃
- `GO_PORTING_SUMMARY.md` - Go 階段總結
- `go/README.md` - Go 程式碼說明
- `go/STRUCTURE.md` - Go 到 C 對應詳解

### 外部資源
- [NetBird 官方專案](https://github.com/netbirdio/netbird)
- [WireGuard 文件](https://www.wireguard.com/)
- [libmnl (netlink)](https://www.netfilter.org/projects/libmnl/)
- [protobuf-c](https://github.com/protobuf-c/protobuf-c)
- [grpc-c](https://github.com/lixiangyun/grpc-c)
- [libnice (ICE)](https://nice.freedesktop.org/)
- [cJSON](https://github.com/DaveGamble/cJSON)

## Linux 系統需求

### Kernel
- Linux 5.6+ (內建 WireGuard 支援)
- 或較舊 kernel + WireGuard kernel module

### 工具
```bash
# WireGuard 工具
apt-get install wireguard-tools

# 網路工具
apt-get install iproute2

# 開發工具
apt-get install build-essential

# Protocol Buffers
apt-get install protobuf-c-compiler libprotobuf-c-dev

# gRPC (可能需要手動編譯)
# libnice (ICE)
apt-get install libnice-dev

# JSON
apt-get install libcjson-dev
```

### Capabilities
Client 需要 `CAP_NET_ADMIN` 權限或 root：
```bash
# 方式 1: root
sudo ./nbc up --setup-key XXX

# 方式 2: capability
sudo setcap cap_net_admin+ep ./nbc
./nbc up --setup-key XXX
```

## 測試環境建議

### 基本測試
1. 兩台 Linux 機器（虛擬機或實體機）
2. 可訪問的 NetBird Management Server
3. 可訪問的 NetBird Signal Server

### 進階測試
1. NAT 環境測試（家用路由器）
2. 跨網段測試
3. OpenWrt 路由器測試

## License

本專案基於 NetBird 官方專案，遵循 BSD-3-Clause License。

詳見 NetBird 官方專案 LICENSE 檔案。

## 貢獻

這是一個移植專案，主要目標是學習和建立 minimal C client。

如果要貢獻給官方 NetBird 專案，請至：
https://github.com/netbirdio/netbird

---

**最後更新**: 2025-11-30
**當前階段**: Go 程式碼準備完成，準備開始 C 實作
**參考**: plan.txt, GO_PORTING_SUMMARY.md
