package encryption

import (
	"crypto/rand"
	"fmt"

	"golang.org/x/crypto/nacl/box"
	"golang.zx2c4.com/wireguard/wgctrl/wgtypes"
)

const nonceSize = 24

// Encrypt encrypts a message using local WireGuard private key and remote peer's public key.
func Encrypt(msg []byte, peerPublicKey wgtypes.Key, privateKey wgtypes.Key) ([]byte, error) {
	nonce, err := genNonce()
	if err != nil {
		return nil, err
	}
	return box.Seal(nonce[:], msg, nonce, toByte32(peerPublicKey), toByte32(privateKey)), nil
}

// Decrypt decrypts a message encrypted by the remote peer using WireGuard private key and remote peer's public key.
func Decrypt(encryptedMsg []byte, peerPublicKey wgtypes.Key, privateKey wgtypes.Key) ([]byte, error) {
	nonce, err := genNonce()
	if err != nil {
		return nil, err
	}
	if len(encryptedMsg) < nonceSize {
		return nil, fmt.Errorf("invalid encrypted message length")
	}
	copy(nonce[:], encryptedMsg[:nonceSize])
	opened, ok := box.Open(nil, encryptedMsg[nonceSize:], nonce, toByte32(peerPublicKey), toByte32(privateKey))
	if !ok {
		return nil, fmt.Errorf("failed to decrypt message from peer %s", peerPublicKey.String())
	}

	return opened, nil
}

func genNonce() (*[nonceSize]byte, error) {
	var nonce [nonceSize]byte
	if _, err := rand.Read(nonce[:]); err != nil {
		return nil, err
	}
	return &nonce, nil
}

func toByte32(key wgtypes.Key) *[32]byte {
	return (*[32]byte)(&key)
}
