# NetBird Go to C Porting - Phase 1 完成總結

## 完成狀態

✅ **Go 程式碼準備階段已完成**

已成功從 NetBird 官方專案複製並整理了 Linux client 所需的最小化 Go 程式碼。

## 統計資訊

- **Go 原始檔案數量**: 71 個
- **總大小**: 696KB
- **主要模組**: 8 個
- **Proto 定義**: 2 個

## 目錄結構

```
/project/netbird/
├── plan.txt                    # 原始移植計劃
├── go/                         # Go 程式碼 (本階段完成)
│   ├── README.md              # Go 程式碼總覽
│   ├── STRUCTURE.md           # 詳細檔案說明
│   ├── go.mod                 # Go 模組定義
│   ├── cmd/                   # CLI 命令 (3 files)
│   ├── internal/              # 核心引擎 (3 files)
│   │   └── routemanager/     # 路由管理
│   ├── iface/                 # WireGuard 介面 (多個檔案)
│   │   ├── device/           # 裝置管理
│   │   ├── configurer/       # WG 設定
│   │   └── wgaddr/           # 位址處理
│   ├── management/            # Management 客戶端
│   │   └── client/
│   ├── signal/                # Signal 客戶端
│   │   └── client/
│   └── proto/                 # Protocol Buffers
│       ├── management.proto
│       └── signalexchange.proto
└── c/                          # C 程式碼 (待實作)
```

## 已複製的核心模組

### 1. Engine (引擎核心)
- `internal/engine.go` - 主要狀態機
- `internal/connect.go` - 連線管理
- **用途**: 管理整個 client 生命週期

### 2. WireGuard Interface (網路介面)
- `iface/iface.go` - 介面定義
- `iface/iface_new_linux.go` - Linux 建立實作
- `iface/iface_destroy_linux.go` - Linux 銷毀實作
- `iface/device/wg_link_linux.go` - netlink 操作
- `iface/device/kernel_module_linux.go` - kernel 檢查
- `iface/configurer/kernel_unix.go` - WG 設定
- **用途**: 建立和管理 WireGuard 隧道

### 3. Management Client (管理伺服器客戶端)
- `management/client/client.go` - 介面定義
- `management/client/grpc.go` - gRPC 實作
- **用途**:
  - 用 setup-key 註冊 peer
  - 同步 peer 列表
  - 取得路由資訊

### 4. Signal Client (信號伺服器客戶端)
- `signal/client/client.go` - 介面定義
- `signal/client/grpc.go` - gRPC 實作
- `signal/client/worker.go` - 背景工作
- **用途**:
  - 交換 SDP offer/answer
  - 交換 ICE candidates
  - P2P 連線建立

### 5. Route Manager (路由管理)
- `routemanager/manager.go` - 管理器介面
- `routemanager/systemops/systemops_linux.go` - Linux 實作
- **用途**: 管理系統路由表

### 6. CLI Commands (命令列)
- `cmd/up.go` - 啟動命令
- `cmd/down.go` - 停止命令
- `cmd/status.go` - 狀態查詢
- **用途**: 使用者介面

### 7. Proto Definitions (協定定義)
- `proto/management.proto` - Management API
- `proto/signalexchange.proto` - Signal API
- **用途**: gRPC 服務定義

## 省略的非必要功能

為了專注於 minimal Linux client，以下功能已省略：

❌ SSH 功能
❌ DNS 管理
❌ Firewall 規則
❌ UI 相關程式碼
❌ Android/iOS/Windows 平台程式碼
❌ 測試檔案 (*_test.go)
❌ Userspace WireGuard (使用 kernel WireGuard)
❌ eBPF 功能
❌ Rosenpass (post-quantum)

## 下一步：C 語言實作

根據 plan.txt 的建議，實作順序如下：

### Phase 1: WireGuard + Route 模組 ⏭️
**目標**: 建立 WireGuard 介面並設定路由

**需要實作**:
```c
// c/wg_iface.h / wg_iface.c
typedef struct wg_iface wg_iface_t;

int wg_iface_create(const nb_config_t *cfg, wg_iface_t **out);
int wg_iface_up(wg_iface_t *iface);
int wg_iface_update_peer(wg_iface_t *iface, ...);
int wg_iface_remove_peer(wg_iface_t *iface, const char *pubkey);
int wg_iface_close(wg_iface_t *iface);

// c/route.h / route.c
typedef struct route_manager route_manager_t;

int route_add(route_manager_t *mgr, const route_config_t *route);
int route_remove(route_manager_t *mgr, const char *network, const char *device);
```

**Linux 實作方式**:
- Option 1 (快速): 使用 `wg` CLI 和 `ip` 命令
- Option 2 (正式): 使用 libmnl (netlink library)

**測試目標**:
能夠手動建立 WireGuard 介面並連接到現有的 WireGuard server

---

### Phase 2: Management Client
**目標**: 實作與 Management Server 的 gRPC 通訊

**需要實作**:
```c
// c/mgmt_client.h / mgmt_client.c
typedef struct mgmt_client mgmt_client_t;

mgmt_client_t* mgmt_client_new(const char *url);
int mgmt_register(mgmt_client_t *c, const char *setup_key, ...);
int mgmt_sync(mgmt_client_t *c, const char *peer_id, ...);
void mgmt_client_free(mgmt_client_t *c);
```

**依賴**:
- protobuf-c: Protocol Buffers C implementation
- grpc-c: gRPC C bindings

**編譯 Proto**:
```bash
protoc --c_out=c/ go/proto/management.proto
```

**測試目標**:
用 setup-key 註冊 peer，在 Management Server UI 看到新的 peer

---

### Phase 3: Engine 整合
**目標**: 整合 WireGuard + Management，建立完整流程

**需要實作**:
```c
// c/engine.h / engine.c
typedef struct nb_engine nb_engine_t;

nb_engine_t* nb_engine_new(const nb_config_t *cfg);
int nb_engine_start(nb_engine_t *eng);
void nb_engine_stop(nb_engine_t *eng);
```

**流程**:
1. 用 setup-key 註冊
2. 建立 WireGuard 介面
3. 從 Management 取得 peer 列表
4. 設定 WireGuard peers
5. 設定路由

**測試目標**:
兩台機器透過 relay server 可以互 ping

---

### Phase 4: Signal Client + ICE
**目標**: 實作 P2P 連線

**需要實作**:
```c
// c/signal_client.h / signal_client.c
typedef struct signal_client signal_client_t;

signal_client_t* signal_client_new(const char *url);
int signal_send(signal_client_t *c, ...);
int signal_receive(signal_client_t *c, signal_message_t *msg_out);
```

**依賴**:
- libnice: ICE/STUN/TURN library

**編譯 Proto**:
```bash
protoc --c_out=c/ go/proto/signalexchange.proto
```

**測試目標**:
兩台機器在同一 LAN 或跨 NAT 環境下 P2P 直連

---

### Phase 5: CLI
**目標**: 實作完整的命令列介面

**需要實作**:
```c
// c/nbc_cli.c
int main(int argc, char *argv[]);
int cmd_up(const char *setup_key, const char *mgmt_url);
int cmd_down(void);
int cmd_status(void);
```

**測試目標**:
```bash
$ nbc up --setup-key XXX --management-url https://...
$ nbc status
$ nbc down
```

---

## 技術棧對應

| 功能 | Go Package | Linux 工具/Library | C 實作 |
|------|------------|-------------------|--------|
| WireGuard | wgctrl | `wg` CLI / libmnl | system() 或 netlink |
| 路由 | netlink | `ip route` / libmnl | system() 或 netlink |
| gRPC | grpc-go | grpc-c | grpc-c |
| Protocol Buffers | protobuf | protobuf-c | protobuf-c |
| ICE | pion/ice | libnice | libnice |
| JSON Config | encoding/json | cJSON | cJSON |
| Logging | log | syslog | syslog 或 fprintf |

## 建議的實作策略

### 快速原型 (Minimal MVP)
1. 使用 shell 命令 (`wg`, `ip route`)
2. 先實作 relay 模式（暫不實作 ICE）
3. 簡單的錯誤處理
4. 單執行緒

**優點**: 快速驗證概念，1-2 週可完成基本功能

### 正式版本
1. 使用 netlink library (libmnl)
2. 實作完整 ICE (libnice)
3. 完善錯誤處理和重連邏輯
4. 多執行緒 (pthread)

**優點**: 高效能，適合 production 環境

## 預期的 C 程式碼結構

```
c/
├── Makefile
├── README.md
├── include/               # 標頭檔
│   ├── engine.h
│   ├── wg_iface.h
│   ├── mgmt_client.h
│   ├── signal_client.h
│   ├── route.h
│   └── config.h
├── src/                   # 原始檔
│   ├── main.c
│   ├── engine.c
│   ├── wg_iface.c
│   ├── mgmt_client.c
│   ├── signal_client.c
│   ├── route.c
│   └── config.c
├── proto/                 # 生成的 proto C 程式碼
│   ├── management.pb-c.h
│   ├── management.pb-c.c
│   ├── signalexchange.pb-c.h
│   └── signalexchange.pb-c.c
└── test/                  # 測試程式
    ├── test_wg_iface.c
    └── test_mgmt_client.c
```

## 參考資料

### Go 程式碼參考
- `go/README.md` - Go 程式碼總覽
- `go/STRUCTURE.md` - 詳細檔案說明和 C 對應
- `go/proto/*.proto` - gRPC 服務定義

### 外部資源
- NetBird 官方: https://github.com/netbirdio/netbird
- WireGuard: https://www.wireguard.com/
- libmnl (netlink): https://www.netfilter.org/projects/libmnl/
- protobuf-c: https://github.com/protobuf-c/protobuf-c
- grpc-c: https://github.com/lixiangyun/grpc-c
- libnice (ICE): https://nice.freedesktop.org/
- cJSON: https://github.com/DaveGamble/cJSON

---

## 總結

Phase 1 (Go 程式碼準備) 已完成 ✅

**成果**:
- 71 個 Go 原始檔已整理至 `go/` 目錄
- 2 個 proto 定義已複製
- 詳細的文件說明 (README.md, STRUCTURE.md)
- 清楚的 C 移植計劃

**下一步**:
開始 Phase 2 - 實作 C 版本的 WireGuard 介面和路由管理

根據 plan.txt，建議採用「快速原型」策略先跑起來，再逐步優化。

---

**日期**: 2025-11-30
**階段**: Go 程式碼準備完成
**下階段**: C 語言實作
