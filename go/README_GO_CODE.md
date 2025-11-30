# Go 程式碼說明

## 重要說明

**這個目錄中的 Go 程式碼不是用來編譯執行的！**

這些程式碼是從 NetBird 官方專案抽取出來的**參考實作**，用於：
1. 理解 NetBird client 的運作邏輯
2. 作為 C 語言移植的參考
3. 查看資料結構和 API 設計

## 為什麼無法編譯？

這些 Go 檔案依賴完整的 NetBird 專案結構，包含：
- `github.com/netbirdio/netbird/client/*` - 數百個檔案
- `github.com/pion/ice/v4` - ICE library
- `golang.zx2c4.com/wireguard/*` - WireGuard library
- 等等...

單獨編譯這些檔案會失敗，因為缺少大量依賴。

## 如何使用這些 Go 程式碼？

### 1. 閱讀和理解

**查看核心邏輯**：
```bash
# 查看 Engine 啟動流程
cat internal/engine.go | less

# 查看 WireGuard 介面建立
cat iface/iface_new_linux.go

# 查看 Management client
cat management/client/grpc.go
```

**搜尋特定功能**：
```bash
# 搜尋 peer 更新邏輯
grep -r "UpdatePeer" .

# 搜尋路由設定
grep -r "AddRoute" .
```

### 2. 對照文件查看

參考 `doc/GO_TO_C_MAPPING.md`，它詳細說明：
- 每個 Go 檔案的功能
- 對應的 C 實作方式
- 需要使用的 C library

### 3. 使用官方專案

如果需要**執行和測試** Go 版本：

```bash
# Clone 官方完整專案
git clone https://github.com/netbirdio/netbird /tmp/netbird-full
cd /tmp/netbird-full

# 編譯官方 client
cd client
go build -o netbird

# 執行
sudo ./netbird up --setup-key YOUR_KEY
```

## 實際開發流程

### Step 1: 閱讀 Go 程式碼理解邏輯
```bash
# 例如：理解 WireGuard 介面如何建立
cat go/iface/iface_new_linux.go
cat go/iface/device/wg_link_linux.go
```

### Step 2: 查看對應文件
```bash
# 查看 C 實作指引
cat doc/GO_TO_C_MAPPING.md
```

### Step 3: 實作 C 版本
```bash
cd c/
# 根據 Go 邏輯實作 C 版本
vim src/wg_iface.c
```

### Step 4: 測試 C 實作
```bash
make
sudo ./nbc up --setup-key YOUR_KEY
```

## 程式碼導覽

### 核心檔案（優先閱讀）

**Engine 核心**：
- `internal/engine.go` (61KB) - 最重要的檔案
  - `Start()` 方法：完整的啟動流程
  - `Stop()` 方法：停止和清理
  - peer 管理邏輯

**WireGuard 介面**：
- `iface/iface.go` - 介面定義
- `iface/iface_new_linux.go` - Linux 建立實作
- `iface/device/wg_link_linux.go` - netlink 操作

**Management Client**：
- `management/client/grpc.go` - gRPC 實作
- `proto/management.proto` - API 定義

**Signal Client**：
- `signal/client/grpc.go` - gRPC 實作
- `proto/signalexchange.proto` - API 定義

**路由管理**：
- `internal/routemanager/manager.go` - 路由管理器
- `internal/routemanager/systemops/systemops_linux.go` - Linux 實作

### 輔助檔案

**CLI**：
- `cmd/up.go` - up 命令實作
- `cmd/down.go` - down 命令實作
- `cmd/status.go` - status 命令實作

**設定**：
- `iface/configurer/kernel_unix.go` - WireGuard 設定
- `iface/wgaddr/address.go` - 位址處理

## 閱讀建議

### 初學者路線

1. **先看 Proto**（最簡單）：
   ```bash
   cat proto/management.proto
   cat proto/signalexchange.proto
   ```
   了解 Management 和 Signal API

2. **看 CLI**（理解使用流程）：
   ```bash
   cat cmd/up.go
   ```
   了解 `netbird up` 做了什麼

3. **看 Engine.Start()**（核心邏輯）：
   ```bash
   grep -A 100 "func.*Start\(" internal/engine.go
   ```
   了解啟動流程

4. **看 WireGuard 介面**：
   ```bash
   cat iface/iface_new_linux.go
   ```
   了解如何建立 WG 介面

### 進階開發者

直接閱讀 `internal/engine.go` 全文，搭配：
- `doc/GO_TO_C_MAPPING.md` - 理解 C 對應
- NetBird 官方文件
- WireGuard 文件

## 常見問題

### Q: 可以修改這些 Go 程式碼嗎？
A: 可以做筆記和標註，但不建議修改。這些是參考用，修改後可能更難對照官方版本。

### Q: 需要學會 Go 語言嗎？
A: 不需要精通，能看懂基本語法即可：
- `func name() {}` - 函式定義
- `if err != nil {}` - 錯誤處理
- `type Foo struct {}` - 結構定義
- Go 的 interface, goroutine, channel 在 C 有不同實作方式

### Q: 如何找到某個功能的實作？
A: 使用 grep：
```bash
# 例如：找 peer 更新
grep -r "UpdatePeer" . --include="*.go"

# 找路由新增
grep -r "AddRoute" . --include="*.go"
```

### Q: Go 和 C 的對應關係？
A: 詳見 `doc/GO_TO_C_MAPPING.md`，例如：
- Go `type Engine struct` → C `typedef struct nb_engine`
- Go `func (e *Engine) Start()` → C `int nb_engine_start(nb_engine_t *e)`
- Go `goroutine` → C `pthread`
- Go `channel` → C `pipe` 或 `pthread condition variable`

## 總結

✅ **這些 Go 程式碼是參考資料**，不是要編譯的
✅ **閱讀和理解**是主要用途
✅ **實際開發在 c/ 目錄**
✅ **參考官方完整專案**來執行測試

---

有問題請參考：
- `../README.md` - 專案總覽
- `../plan.txt` - 實作計劃
- `../doc/GO_TO_C_MAPPING.md` - Go 到 C 對應指南
