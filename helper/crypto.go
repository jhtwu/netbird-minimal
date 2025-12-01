package main

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"

	"golang.org/x/crypto/curve25519"
	"golang.org/x/crypto/nacl/box"
)

// NetBird uses WireGuard keys for message encryption
// Algorithm: NaCl box (Curve25519 + XSalsa20-Poly1305)

const (
	KeySize   = 32 // WireGuard uses 32-byte keys
	NonceSize = 24 // NaCl box nonce size
)

// DecodeWGKey decodes a base64-encoded WireGuard key
func DecodeWGKey(keyStr string) ([]byte, error) {
	key, err := base64.StdEncoding.DecodeString(keyStr)
	if err != nil {
		return nil, fmt.Errorf("decode base64: %w", err)
	}
	if len(key) != KeySize {
		return nil, fmt.Errorf("invalid key size: got %d, want %d", len(key), KeySize)
	}
	return key, nil
}

// EncodeWGKey encodes a WireGuard key to base64
func EncodeWGKey(key []byte) string {
	return base64.StdEncoding.EncodeToString(key)
}

// toArray32 converts a byte slice to a 32-byte array
func toArray32(key []byte) (*[32]byte, error) {
	if len(key) != 32 {
		return nil, fmt.Errorf("key must be 32 bytes, got %d", len(key))
	}
	var arr [32]byte
	copy(arr[:], key)
	return &arr, nil
}

// EncryptMessage encrypts a message using NaCl box (XSalsa20-Poly1305)
// Returns: nonce + ciphertext (concatenated)
func EncryptMessage(plaintext []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
	// Generate random nonce
	var nonce [NonceSize]byte
	if _, err := rand.Read(nonce[:]); err != nil {
		return nil, fmt.Errorf("generate nonce: %w", err)
	}

	// Convert keys to [32]byte arrays
	privKey, err := toArray32(ourPrivKey)
	if err != nil {
		return nil, fmt.Errorf("convert private key: %w", err)
	}
	pubKey, err := toArray32(peerPubKey)
	if err != nil {
		return nil, fmt.Errorf("convert public key: %w", err)
	}

	// Encrypt using NaCl box
	// box.Seal prepends nonce to ciphertext
	encrypted := box.Seal(nonce[:], plaintext, &nonce, pubKey, privKey)
	return encrypted, nil
}

// DecryptMessage decrypts a message using NaCl box (XSalsa20-Poly1305)
// Input: nonce + ciphertext (concatenated)
func DecryptMessage(encrypted []byte, ourPrivKey, peerPubKey []byte) ([]byte, error) {
	if len(encrypted) < NonceSize {
		return nil, fmt.Errorf("encrypted message too short")
	}

	// Extract nonce
	var nonce [NonceSize]byte
	copy(nonce[:], encrypted[:NonceSize])

	// Convert keys to [32]byte arrays
	privKey, err := toArray32(ourPrivKey)
	if err != nil {
		return nil, fmt.Errorf("convert private key: %w", err)
	}
	pubKey, err := toArray32(peerPubKey)
	if err != nil {
		return nil, fmt.Errorf("convert public key: %w", err)
	}

	// Decrypt using NaCl box
	plaintext, ok := box.Open(nil, encrypted[NonceSize:], &nonce, pubKey, privKey)
	if !ok {
		return nil, fmt.Errorf("decrypt: authentication failed")
	}

	return plaintext, nil
}

// EncryptForServer encrypts a message for the management server
// Uses our WG private key and server's public key
func EncryptForServer(plaintext []byte, ourPrivKeyStr, serverPubKeyStr string) ([]byte, error) {
	ourPrivKey, err := DecodeWGKey(ourPrivKeyStr)
	if err != nil {
		return nil, fmt.Errorf("decode our private key: %w", err)
	}

	serverPubKey, err := DecodeWGKey(serverPubKeyStr)
	if err != nil {
		return nil, fmt.Errorf("decode server public key: %w", err)
	}

	encrypted, err := EncryptMessage(plaintext, ourPrivKey, serverPubKey)
	if err != nil {
		return nil, fmt.Errorf("encrypt: %w", err)
	}

	return encrypted, nil
}

// DecryptFromServer decrypts a message from the management server
func DecryptFromServer(encrypted []byte, ourPrivKeyStr, serverPubKeyStr string) ([]byte, error) {
	ourPrivKey, err := DecodeWGKey(ourPrivKeyStr)
	if err != nil {
		return nil, fmt.Errorf("decode our private key: %w", err)
	}

	serverPubKey, err := DecodeWGKey(serverPubKeyStr)
	if err != nil {
		return nil, fmt.Errorf("decode server public key: %w", err)
	}

	plaintext, err := DecryptMessage(encrypted, ourPrivKey, serverPubKey)
	if err != nil {
		return nil, fmt.Errorf("decrypt: %w", err)
	}

	return plaintext, nil
}

// DerivePublicKey derives the public key from a private key
func DerivePublicKey(privKeyStr string) (string, error) {
	privKey, err := DecodeWGKey(privKeyStr)
	if err != nil {
		return "", fmt.Errorf("decode private key: %w", err)
	}

	pubKey, err := curve25519.X25519(privKey, curve25519.Basepoint)
	if err != nil {
		return "", fmt.Errorf("derive public key: %w", err)
	}

	return EncodeWGKey(pubKey), nil
}
