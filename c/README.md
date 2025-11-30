# NetBird Minimal C Client - Phase 1~3 Prototype

WireGuard Interface + Route Management + JSON 設定 + 基本 CLI（僅手動 peer/路由）

## 狀態

- ✅ Phase 1：WireGuard Interface + Route Manager（使用 shell 命令原型）
- ✅ Phase 2：JSON 設定（cJSON，支援 CamelCase + snake_case）
- ✅ Phase 3：Engine + CLI（手動 peer/路由管理）
- ⏳ 未完成：Management/Signal gRPC、setup-key/自動註冊、ICE/P2P

## 實作內容

1. **WireGuard Interface** (`wg_iface.c`)
   - 建立/刪除 WireGuard 網路介面
   - 管理 peers (新增/更新/刪除)
   - 產生 WireGuard keys

2. **Route Management** (`route.c`)
   - 新增/移除路由規則
   - NAT masquerading 支援

3. **Configuration** (`config.c`)
   - JSON 讀寫，支援 NetBird CamelCase 與 snake_case 鍵名
   - 預設介面名稱改為 `wtnb0`，避免覆蓋既有 `wt0`

4. **Engine + CLI** (`engine.c`, `main.c`)
   - `up / down / status / add-peer` 基本命令
   - 僅支援手動管理 peers/路由（尚無 management/signal）

## 編譯

需求：
```bash
sudo apt-get install wireguard-tools iproute2 iptables libcjson-dev build-essential
```

編譯：
```bash
cd c/
make
```

輸出 (`build/`)：
- `netbird-client` - CLI
- `test_wg_iface`, `test_route`, `test_config`, `test_engine`

## 測試（需 root）

```bash
cd c/
sudo ./build/test_wg_iface     # WireGuard 介面
sudo ./build/test_route        # 路由
sudo ./build/test_config       # JSON 讀寫
sudo ./build/test_engine       # Engine 整合
# sudo ./build/test_cli_workflow.sh  # 手動 CLI workflow（使用獨立介面名 wtnb-cli0）
```

## API 範例

### WireGuard Interface
```c
nb_config_t *cfg;
config_new_default(&cfg);
cfg->wg_private_key = strdup("YOUR_PRIVATE_KEY");
cfg->wg_address = strdup("100.64.0.5/16");
cfg->wg_listen_port = 51820;

wg_iface_t *iface;
wg_iface_create(cfg, &iface);
wg_iface_up(iface);
wg_iface_update_peer(iface, "PEER_PUBLIC_KEY",
                     "100.64.0.6/32", 25, "1.2.3.4:51820", NULL);
wg_iface_destroy(iface);
wg_iface_free(iface);
config_free(cfg);
```

### Route Management
```c
route_manager_t *mgr = route_manager_new("wtnb0");
route_config_t route = {
    .network = "10.0.0.0/8",
    .device = "wtnb0",
    .metric = 100,
    .masquerade = 0
};
route_add(mgr, &route);
route_remove(mgr, "10.0.0.0/8");
route_manager_free(mgr);
```

## 已知限制 / TODO

1. ⚠️  使用 shell 命令（`ip`, `wg`, `iptables`），需改為 netlink/libwg。
2. ⚠️  尚未實作 management/signal gRPC、setup-key/自動註冊、ICE/P2P；目前僅手動 peers/路由。
3. ⚠️  CLI/測試會建立/刪除介面，預設 `wtnb0`、測試 `wtnb-cli0` 以避免干擾既有 `wt0`，仍建議在隔離環境執行。

## 手動測試提示

```bash
# 建立/查看/刪除介面（請替換介面名，預設 wtnb0）
sudo ip link add dev wtnb0 type wireguard
sudo ip address add 100.64.0.100/16 dev wtnb0
sudo wg set wtnb0 private-key <(echo "YOUR_KEY") listen-port 51820
sudo ip link set wtnb0 up
sudo wg show wtnb0
sudo ip link del wtnb0
```

## 參考

- Go 參考碼：`../go/iface/iface_new_linux.go`, `../go/internal/routemanager/systemops/systemops_linux.go`
- 文件：`../doc/GO_TO_C_MAPPING.md`, `../plan.txt`
