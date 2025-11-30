module github.com/netbirdio/netbird-minimal

go 1.21

// 這是 minimal 版本，只包含必要的依賴項
// 完整依賴請參考官方 NetBird 專案

require (
	github.com/pion/ice/v2 v2.3.11
	google.golang.org/grpc v1.60.0
	google.golang.org/protobuf v1.31.0
	github.com/vishvananda/netlink v1.1.0
	golang.zx2c4.com/wireguard/wgctrl v0.0.0-20230429144221-925a1e7659e6
)

// 注意：這個 go.mod 僅供參考
// C 移植將使用對應的 C library 而非 Go 依賴
