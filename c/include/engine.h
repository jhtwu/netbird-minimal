/**
 * engine.h - NetBird client engine
 *
 * Reference: go/internal/engine.go
 *
 * The engine is the main controller that coordinates:
 * - WireGuard interface management
 * - Route management
 * - Configuration
 * - (Future: Management and Signal client communication)
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#ifndef NB_ENGINE_H
#define NB_ENGINE_H

#include "config.h"
#include "wg_iface.h"
#include "route.h"
#include "mgmt_client.h"

/**
 * Engine structure
 *
 * Reference: Go Engine struct
 */
typedef struct nb_engine {
    /* Configuration */
    nb_config_t *config;

    /* WireGuard interface */
    wg_iface_t *wg_iface;

    /* Route manager */
    route_manager_t *route_mgr;

    /* Management client (Phase 4) */
    mgmt_client_t *mgmt_client;

    /* State */
    int running;

    /* Future: Signal client will be added here in Phase 4 */
} nb_engine_t;

/**
 * Peer information (simplified for Phase 1/3)
 *
 * In real implementation, this would come from Management server.
 * For now, we'll use manual configuration.
 */
typedef struct {
    char *public_key;        /* Peer's WireGuard public key */
    char **allowed_ips;      /* Array of allowed IP CIDRs */
    int allowed_ips_count;
    char *endpoint;          /* Peer endpoint (IP:port) */
    int keepalive;           /* Persistent keepalive interval */
} nb_peer_info_t;

/**
 * Create a new engine instance
 *
 * Reference: Go NewEngine()
 *
 * @param config Configuration (ownership transferred to engine)
 * @return Engine instance, NULL on failure
 */
nb_engine_t* nb_engine_new(nb_config_t *config);

/**
 * Start the engine
 *
 * Reference: Go Engine.Start()
 *
 * This function:
 * 1. Creates WireGuard interface
 * 2. Brings interface up
 * 3. Sets up routes (if configured)
 * 4. (Future: Connects to Management/Signal servers)
 *
 * @param engine Engine instance
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int nb_engine_start(nb_engine_t *engine);

/**
 * Start engine with management registration (Phase 4)
 *
 * This function:
 * 1. Creates management client
 * 2. Registers with setup key (gets peer list)
 * 3. Creates WireGuard interface
 * 4. Adds peers from management
 * 5. Sets up routes
 *
 * @param engine Engine instance
 * @param setup_key Setup key for registration (NULL to skip registration)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int nb_engine_start_with_mgmt(nb_engine_t *engine, const char *setup_key);

/**
 * Stop the engine
 *
 * Reference: Go Engine.Stop()
 *
 * This function:
 * 1. Removes routes
 * 2. Removes peers
 * 3. Brings interface down
 * 4. Destroys interface
 * 5. (Future: Disconnects from servers)
 *
 * @param engine Engine instance
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int nb_engine_stop(nb_engine_t *engine);

/**
 * Add a peer to the engine
 *
 * Reference: Go Engine peer management
 *
 * @param engine Engine instance
 * @param peer Peer information
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int nb_engine_add_peer(nb_engine_t *engine, const nb_peer_info_t *peer);

/**
 * Remove a peer from the engine
 *
 * @param engine Engine instance
 * @param public_key Peer's public key to remove
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int nb_engine_remove_peer(nb_engine_t *engine, const char *public_key);

/**
 * Free engine instance
 *
 * Note: This does NOT stop the engine.
 * Call nb_engine_stop() first if needed.
 *
 * @param engine Engine instance to free
 */
void nb_engine_free(nb_engine_t *engine);

/**
 * Helper: Free peer info structure
 *
 * @param peer Peer info to free
 */
void nb_peer_info_free(nb_peer_info_t *peer);

#endif /* NB_ENGINE_H */
