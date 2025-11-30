# NetBird Minimal C Client - Phase 1

WireGuard Interface + Route Management (Quick Prototype)

## 狀態

✅ **Phase 1 完成** - WireGuard Interface + Route Manager 已實作並編譯成功

## 實作內容

### 模組

1. **WireGuard Interface** (`wg_iface.c`)
   - 建立/刪除 WireGuard 網路介面
   - 管理 peers (新增/更新/刪除)
   - 產生 WireGuard keys

2. **Route Management** (`route.c`)
   - 新增/移除路由規則
   - NAT masquerading 支援

3. **Configuration** (`config.c`)
   - 基本設定結構 (JSON 支援待 Phase 2)

### 實作方式

**快速原型版本** - 使用 shell 命令：
- `ip link` - 管理網路介面
- `ip address` - 設定 IP 位址
- `ip route` - 管理路由
- `wg` - WireGuard 設定工具
- `iptables` - NAT 規則

✅ 優點：快速實作，易於除錯
⚠️  缺點：效能較低，需要 fork/exec

**正式版本** (Phase 6 優化時實作)：
- 使用 libmnl (netlink library)
- 直接操作 kernel，無需外部命令
- 更高效能

## 編譯

### 需求

```bash
# 系統工具
sudo apt-get install wireguard-tools iproute2 iptables

# 編譯工具
sudo apt-get install build-essential
```

### 編譯

```bash
cd c/
make
```

編譯成功後會在 `build/` 目錄產生測試程式：
- `build/test_wg_iface` - WireGuard 介面測試
- `build/test_route` - 路由管理測試

## 測試

**注意：需要 root 權限**（建立網路介面需要）

### Test 1: WireGuard Interface

```bash
cd c/
sudo make test
```

或手動執行：

```bash
sudo ./build/test_wg_iface
```

測試項目：
1. 產生 WireGuard private key
2. 衍生 public key
3. 建立 WireGuard 介面
4. 啟動介面
5. 新增 peer
6. 移除 peer
7. 關閉介面
8. 刪除介面

### Test 2: Route Management

```bash
sudo ./build/test_route
```

測試項目：
1. 建立 WireGuard 介面 (setup)
2. 建立 route manager
3. 新增路由 (10.0.0.0/24)
4. 新增路由 (10.1.0.0/16)
5. 查看路由表
6. 移除單一路由
7. 移除所有路由

## 專案結構

```
c/
├── Makefile                 # 建置檔案
├── README.md                # 本檔案
│
├── include/                 # 標頭檔
│   ├── common.h            # 共用定義
│   ├── config.h            # 設定管理
│   ├── wg_iface.h          # WireGuard 介面
│   └── route.h             # 路由管理
│
├── src/                     # 原始檔
│   ├── common.c
│   ├── config.c
│   ├── wg_iface.c
│   └── route.c
│
├── test/                    # 測試程式
│   ├── test_wg_iface.c
│   └── test_route.c
│
└── build/                   # 編譯輸出 (自動產生)
    ├── test_wg_iface
    └── test_route
```

## API 範例

### WireGuard Interface

```c
#include "wg_iface.h"
#include "config.h"

/* 建立設定 */
nb_config_t *cfg;
config_new_default(&cfg);
cfg->wg_private_key = strdup("YOUR_PRIVATE_KEY");
cfg->wg_address = strdup("100.64.0.5/16");
cfg->wg_listen_port = 51820;

/* 建立介面 */
wg_iface_t *iface;
wg_iface_create(cfg, &iface);

/* 啟動 */
wg_iface_up(iface);

/* 新增 peer */
wg_iface_update_peer(
    iface,
    "PEER_PUBLIC_KEY",
    "100.64.0.6/32",  // allowed IPs
    25,                // keepalive
    "1.2.3.4:51820",  // endpoint
    NULL               // no pre-shared key
);

/* 清理 */
wg_iface_destroy(iface);
wg_iface_free(iface);
config_free(cfg);
```

### Route Management

```c
#include "route.h"

/* 建立 route manager */
route_manager_t *mgr = route_manager_new("wt0");

/* 新增路由 */
route_config_t route = {
    .network = "10.0.0.0/8",
    .device = "wt0",
    .metric = 100,
    .masquerade = 0
};
route_add(mgr, &route);

/* 移除路由 */
route_remove(mgr, "10.0.0.0/8");

/* 清理 */
route_manager_free(mgr);
```

## 已知限制 (Phase 1)

1. ⚠️  **使用 shell 命令**：效能較低，需要 Phase 6 優化
2. ⚠️  **無 JSON 支援**：config.c 只有基本功能，Phase 2 加入 cJSON
3. ⚠️  **無錯誤重試**：失敗即返回錯誤，無自動重試
4. ⚠️  **無 daemon 模式**：測試程式是單次執行，Phase 5 加入 CLI

## 下一步：Phase 2

Management Client 實作：
- [ ] 編譯 `management.proto` 為 C 程式碼
- [ ] 實作 `mgmt_client.c` (gRPC)
- [ ] 加入 cJSON 支援到 `config.c`
- [ ] 實作 setup-key 註冊流程
- [ ] 測試與 Management Server 通訊

參考: `../plan.txt`, `../doc/GO_TO_C_MAPPING.md`

## 除錯

### 啟用 DEBUG 訊息

程式碼已包含 `NB_LOG_DEBUG` 輸出，顯示執行的命令。

### 手動測試

```bash
# 建立 WireGuard 介面
sudo ip link add dev wt0 type wireguard
sudo ip address add 100.64.0.100/16 dev wt0
sudo wg set wt0 private-key <(echo "YOUR_KEY") listen-port 51820
sudo ip link set wt0 up

# 查看狀態
sudo wg show wt0

# 清理
sudo ip link del wt0
```

## 參考

- Go 程式碼: `../go/iface/iface_new_linux.go`
- Go 程式碼: `../go/internal/routemanager/systemops/systemops_linux.go`
- 對應文件: `../doc/GO_TO_C_MAPPING.md`
- 計劃文件: `../plan.txt`

---

**Author**: Claude
**Date**: 2025-11-30
**Phase**: 1 of 5
**Status**: ✅ Completed
