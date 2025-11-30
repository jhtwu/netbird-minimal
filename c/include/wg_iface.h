/**
 * wg_iface.h - WireGuard interface management
 *
 * Reference: go/iface/iface.go, go/iface/iface_new_linux.go
 *
 * This module manages WireGuard network interfaces on Linux.
 * Quick prototype version uses shell commands (wg, ip).
 * Production version should use netlink API.
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#ifndef NB_WG_IFACE_H
#define NB_WG_IFACE_H

#include "config.h"

/* Forward declaration */
typedef struct wg_iface wg_iface_t;

/**
 * WireGuard interface structure
 *
 * Reference: Go WGIface interface
 */
struct wg_iface {
    char *name;              /* Interface name, e.g., "wtnb0" */
    char *address;           /* IP address with CIDR, e.g., "100.64.0.5/16" */
    char *private_key;       /* WireGuard private key */
    int listen_port;         /* UDP listen port */

    /* State */
    int created;             /* 1 if interface created */
    int up;                  /* 1 if interface is up */
};

/**
 * Create a new WireGuard interface
 *
 * Reference: Go NewWGIFace() and Create()
 *
 * This function:
 * 1. Creates a WireGuard network interface
 * 2. Assigns IP address
 * 3. Sets private key and listen port
 * 4. Does NOT bring the interface up (call wg_iface_up() for that)
 *
 * @param cfg Configuration containing WG parameters
 * @param iface_out Output interface structure (allocated by this function)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_iface_create(const nb_config_t *cfg, wg_iface_t **iface_out);

/**
 * Bring WireGuard interface up
 *
 * Reference: Go WGIface.Up()
 *
 * @param iface WireGuard interface
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_iface_up(wg_iface_t *iface);

/**
 * Bring WireGuard interface down
 *
 * Reference: Go WGIface.Down()
 *
 * @param iface WireGuard interface
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_iface_down(wg_iface_t *iface);

/**
 * Update or add a peer to the WireGuard interface
 *
 * Reference: Go WGIface.UpdatePeer()
 *
 * @param iface WireGuard interface
 * @param peer_pubkey Peer's public key (base64)
 * @param allowed_ips Comma-separated CIDR list, e.g., "100.64.0.6/32,10.0.0.0/24"
 * @param persistent_keepalive Keepalive interval in seconds (0 to disable)
 * @param endpoint Peer endpoint "IP:port" (NULL if unknown)
 * @param preshared_key Pre-shared key (NULL if none)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_iface_update_peer(
    wg_iface_t *iface,
    const char *peer_pubkey,
    const char *allowed_ips,
    int persistent_keepalive,
    const char *endpoint,
    const char *preshared_key
);

/**
 * Remove a peer from the WireGuard interface
 *
 * Reference: Go WGIface.RemovePeer()
 *
 * @param iface WireGuard interface
 * @param peer_pubkey Peer's public key to remove
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_iface_remove_peer(wg_iface_t *iface, const char *peer_pubkey);

/**
 * Destroy the WireGuard interface
 *
 * Reference: Go WGIface.Close()
 *
 * This function:
 * 1. Brings interface down
 * 2. Removes all peers
 * 3. Deletes the network interface
 *
 * @param iface WireGuard interface to destroy
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_iface_destroy(wg_iface_t *iface);

/**
 * Free WireGuard interface structure
 *
 * Note: This only frees memory, it does NOT destroy the interface.
 * Call wg_iface_destroy() first if needed.
 *
 * @param iface WireGuard interface to free
 */
void wg_iface_free(wg_iface_t *iface);

/**
 * Get WireGuard public key from private key
 *
 * @param private_key Private key (base64)
 * @param public_key_out Output public key (allocated by this function)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_get_public_key(const char *private_key, char **public_key_out);

/**
 * Generate a new WireGuard private key
 *
 * @param private_key_out Output private key (allocated by this function)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int wg_generate_private_key(char **private_key_out);

#endif /* NB_WG_IFACE_H */
