/**
 * mgmt_client.h - Management client (stub/mock implementation)
 *
 * This is a simplified mock implementation that returns static peer configuration.
 * For full implementation with gRPC, see Phase 4 detailed plan.
 *
 * Reference: go/miniclient/management.go
 *
 * Author: Claude
 * Date: 2025-12-01
 */

#ifndef MGMT_CLIENT_H
#define MGMT_CLIENT_H

#include "common.h"

/* Management client structure */
typedef struct mgmt_client mgmt_client_t;

/* Peer information from management server */
typedef struct {
    char *id;              /* Peer ID */
    char *public_key;      /* WireGuard public key */
    char *endpoint;        /* IP:port */
    char *allowed_ips;     /* CIDR notation */
} mgmt_peer_t;

/* Network configuration from management */
typedef struct {
    mgmt_peer_t *peers;       /* Array of peers */
    int peer_count;

    char **routes;            /* Array of route CIDRs */
    int route_count;

    char *wg_private_key;     /* Our WireGuard private key (if assigned) */
    char *wg_address;         /* Our WireGuard IP address */
} mgmt_config_t;

/**
 * Create new management client
 *
 * @param url Management server URL (e.g. "https://api.netbird.io:443")
 * @return Client instance or NULL on error
 */
mgmt_client_t* mgmt_client_new(const char *url);

/**
 * Register with management server using setup key
 *
 * This is a STUB implementation that returns mock data.
 * For production, this should:
 * 1. Connect to management server via gRPC
 * 2. Send setup key for authentication
 * 3. Receive peer list and network configuration
 *
 * @param client Management client
 * @param setup_key Setup key for registration
 * @param config_out Output: Network configuration (caller must free with mgmt_config_free)
 * @return NB_SUCCESS on success, error code on failure
 */
int mgmt_register(mgmt_client_t *client, const char *setup_key, mgmt_config_t **config_out);

/**
 * Sync with management server (get updated peer list)
 *
 * STUB: Currently returns empty update
 *
 * @param client Management client
 * @param config_out Output: Updated configuration
 * @return NB_SUCCESS on success, error code on failure
 */
int mgmt_sync(mgmt_client_t *client, mgmt_config_t **config_out);

/**
 * Free management configuration
 */
void mgmt_config_free(mgmt_config_t *config);

/**
 * Free management client
 */
void mgmt_client_free(mgmt_client_t *client);

#endif /* MGMT_CLIENT_H */
