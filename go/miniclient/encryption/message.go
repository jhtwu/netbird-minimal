package encryption

import (
	pb "github.com/golang/protobuf/proto"
	"golang.zx2c4.com/wireguard/wgctrl/wgtypes"
)

// EncryptMessage marshals and encrypts a protobuf message.
func EncryptMessage(remotePubKey wgtypes.Key, ourPrivateKey wgtypes.Key, message pb.Message) ([]byte, error) {
	byteResp, err := pb.Marshal(message)
	if err != nil {
		return nil, err
	}
	return Encrypt(byteResp, remotePubKey, ourPrivateKey)
}

// DecryptMessage decrypts into the provided protobuf message.
func DecryptMessage(remotePubKey wgtypes.Key, ourPrivateKey wgtypes.Key, encryptedMessage []byte, message pb.Message) error {
	decrypted, err := Decrypt(encryptedMessage, remotePubKey, ourPrivateKey)
	if err != nil {
		return err
	}
	return pb.Unmarshal(decrypted, message)
}
