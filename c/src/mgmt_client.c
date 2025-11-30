/**
 * mgmt_client.c - Management client implementation (stub/mock)
 *
 * This is a STUB implementation for Phase 4 testing.
 * Returns mock peer data to enable end-to-end testing without real management server.
 *
 * For production (Phase 4 complete):
 * - Connect to management server via gRPC
 * - Implement Protocol Buffers messages
 * - Handle encrypted sync stream
 *
 * Reference: go/miniclient/management.go
 *
 * Author: Claude
 * Date: 2025-12-01
 */

#include "mgmt_client.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct mgmt_client {
    char *url;
    int connected;
};

mgmt_client_t* mgmt_client_new(const char *url) {
    if (!url) {
        NB_LOG_ERROR("Invalid management URL");
        return NULL;
    }

    mgmt_client_t *client = calloc(1, sizeof(mgmt_client_t));
    if (!client) {
        NB_LOG_ERROR("calloc failed");
        return NULL;
    }

    client->url = strdup(url);
    client->connected = 0;

    NB_LOG_INFO("Management client created (stub mode): %s", url);
    return client;
}

int mgmt_register(mgmt_client_t *client, const char *setup_key, mgmt_config_t **config_out) {
    if (!client || !config_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("========================================");
    NB_LOG_INFO("  Management Registration (STUB)");
    NB_LOG_INFO("========================================");
    NB_LOG_INFO("  Server:    %s", client->url);
    if (setup_key && setup_key[0]) {
        NB_LOG_INFO("  Setup Key: %s", setup_key);
    } else {
        NB_LOG_WARN("  Setup Key: (none - using demo data)");
    }

    /* Allocate config */
    mgmt_config_t *config = calloc(1, sizeof(mgmt_config_t));
    if (!config) {
        NB_LOG_ERROR("calloc failed");
        return NB_ERROR_SYSTEM;
    }

    /*
     * STUB: Return demo peer and route
     *
     * In real implementation:
     * 1. Connect to gRPC management API
     * 2. Send setup key for authentication
     * 3. Receive actual peer list from server
     * 4. Receive our assigned WireGuard key and IP
     */

    /* Demo peer - represents another machine in the network */
    config->peer_count = 1;
    config->peers = calloc(1, sizeof(mgmt_peer_t));

    char peer_id[64];
    snprintf(peer_id, sizeof(peer_id), "peer-stub-%ld", time(NULL));

    config->peers[0].id = strdup(peer_id);
    config->peers[0].public_key = strdup("DEMO_PEER_PUBKEY_PLACEHOLDER_1234567890ABCDEF=");
    config->peers[0].endpoint = strdup("203.0.113.50:51820");  /* TEST-NET-3 IP */
    config->peers[0].allowed_ips = strdup("100.64.1.0/24");

    NB_LOG_INFO("  Peer:      %s", config->peers[0].id);
    NB_LOG_INFO("    PubKey:  %s", config->peers[0].public_key);
    NB_LOG_INFO("    Endpoint: %s", config->peers[0].endpoint);
    NB_LOG_INFO("    Networks: %s", config->peers[0].allowed_ips);

    /* Demo route */
    config->route_count = 1;
    config->routes = calloc(1, sizeof(char*));
    config->routes[0] = strdup("10.20.0.0/16");

    NB_LOG_INFO("  Routes:    %s", config->routes[0]);

    /* No WG key assignment (use existing config) */
    config->wg_private_key = NULL;
    config->wg_address = NULL;

    NB_LOG_INFO("========================================");
    NB_LOG_WARN("NOTE: This is STUB data for testing!");
    NB_LOG_WARN("For real peers, implement full gRPC client.");
    NB_LOG_INFO("========================================");

    client->connected = 1;
    *config_out = config;

    return NB_SUCCESS;
}

int mgmt_sync(mgmt_client_t *client, mgmt_config_t **config_out) {
    if (!client || !config_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Management sync (stub - no changes)");

    /* STUB: Return empty config (no updates) */
    mgmt_config_t *config = calloc(1, sizeof(mgmt_config_t));
    if (!config) {
        return NB_ERROR_SYSTEM;
    }

    config->peer_count = 0;
    config->peers = NULL;
    config->route_count = 0;
    config->routes = NULL;

    *config_out = config;
    return NB_SUCCESS;
}

void mgmt_config_free(mgmt_config_t *config) {
    if (!config) return;

    /* Free peers */
    for (int i = 0; i < config->peer_count; i++) {
        free(config->peers[i].id);
        free(config->peers[i].public_key);
        free(config->peers[i].endpoint);
        free(config->peers[i].allowed_ips);
    }
    free(config->peers);

    /* Free routes */
    nb_free_string_array(config->routes, config->route_count);

    /* Free WG config */
    free(config->wg_private_key);
    free(config->wg_address);

    free(config);
}

void mgmt_client_free(mgmt_client_t *client) {
    if (!client) return;

    free(client->url);
    free(client);

    NB_LOG_INFO("Management client freed");
}
