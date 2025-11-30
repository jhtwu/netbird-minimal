# NetBird C Client 實作檢查清單

本檢查清單用於追蹤 C 實作的進度。

## Phase 1: WireGuard + Route 模組 ⏭️

### WireGuard Interface (c/wg_iface.c)
- [ ] 定義 `wg_iface_t` 結構
- [ ] 實作 `wg_iface_create()` - 建立 WireGuard 介面
  - [ ] 使用 `ip link add` 建立介面
  - [ ] 設定 IP 位址
  - [ ] 設定 private key
  - [ ] 設定 listen port
- [ ] 實作 `wg_iface_up()` - 啟動介面
- [ ] 實作 `wg_iface_update_peer()` - 更新 peer 設定
- [ ] 實作 `wg_iface_remove_peer()` - 移除 peer
- [ ] 實作 `wg_iface_destroy()` - 銷毀介面
- [ ] 單元測試

### Route Manager (c/route.c)
- [ ] 定義 `route_manager_t` 結構
- [ ] 實作 `route_manager_new()`
- [ ] 實作 `route_add()` - 新增路由
- [ ] 實作 `route_remove()` - 移除路由
- [ ] 單元測試

### 測試目標
- [ ] 能建立 WireGuard 介面並連接到現有 WireGuard server

---

## Phase 2: Management Client
- [ ] 編譯 management.proto
- [ ] 實作 `mgmt_client.c`
- [ ] 實作 `config.c` (JSON config)
- [ ] 測試: 用 setup-key 註冊成功

---

## Phase 3: Engine 整合
- [ ] 實作 `engine.c`
- [ ] 整合 Phase 1 + 2
- [ ] 測試: 兩台機器透過 relay 互 ping

---

## Phase 4: Signal Client + ICE
- [ ] 編譯 signalexchange.proto
- [ ] 實作 `signal_client.c`
- [ ] 整合 libnice (ICE)
- [ ] 測試: P2P 直連

---

## Phase 5: CLI
- [ ] 實作 `nbc_cli.c` (main)
- [ ] 實作 up/down/status 命令
- [ ] Daemon 模式
- [ ] 測試: 完整 CLI 工作流程

詳細檢查項目請見完整版文件。
