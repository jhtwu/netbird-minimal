/**
 * engine.c - NetBird client engine implementation
 *
 * Reference: go/internal/engine.go
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "engine.h"
#include "common.h"

nb_engine_t* nb_engine_new(nb_config_t *config) {
    if (!config) {
        NB_LOG_ERROR("Invalid config");
        return NULL;
    }

    nb_engine_t *engine = calloc(1, sizeof(nb_engine_t));
    if (!engine) {
        NB_LOG_ERROR("calloc failed");
        return NULL;
    }

    engine->config = config;
    engine->running = 0;

    NB_LOG_INFO("Engine created");
    return engine;
}

int nb_engine_start(nb_engine_t *engine) {
    if (!engine || !engine->config) {
        NB_LOG_ERROR("Invalid engine");
        return NB_ERROR_INVALID;
    }

    if (engine->running) {
        NB_LOG_WARN("Engine already running");
        return NB_SUCCESS;
    }

    int ret;

    NB_LOG_INFO("Starting NetBird engine...");

    /* Validate configuration */
    if (!engine->config->wg_private_key) {
        NB_LOG_ERROR("No WireGuard private key in configuration");
        return NB_ERROR_INVALID;
    }

    if (!engine->config->wg_address) {
        NB_LOG_ERROR("No WireGuard address in configuration");
        return NB_ERROR_INVALID;
    }

    /* Step 1: Create WireGuard interface */
    NB_LOG_INFO("Step 1: Creating WireGuard interface...");
    ret = wg_iface_create(engine->config, &engine->wg_iface);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to create WireGuard interface");
        return ret;
    }

    /* Step 2: Bring interface up */
    NB_LOG_INFO("Step 2: Bringing interface up...");
    ret = wg_iface_up(engine->wg_iface);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to bring interface up");
        wg_iface_destroy(engine->wg_iface);
        wg_iface_free(engine->wg_iface);
        engine->wg_iface = NULL;
        return ret;
    }

    /* Step 3: Create route manager */
    NB_LOG_INFO("Step 3: Creating route manager...");
    engine->route_mgr = route_manager_new(engine->wg_iface->name);
    if (!engine->route_mgr) {
        NB_LOG_ERROR("Failed to create route manager");
        nb_engine_stop(engine);
        return NB_ERROR_SYSTEM;
    }

    engine->running = 1;

    NB_LOG_INFO("========================================");
    NB_LOG_INFO("  NetBird engine started successfully");
    NB_LOG_INFO("  Interface: %s", engine->wg_iface->name);
    NB_LOG_INFO("  Address:   %s", engine->wg_iface->address);
    NB_LOG_INFO("  Port:      %d", engine->wg_iface->listen_port);
    NB_LOG_INFO("========================================");

    return NB_SUCCESS;
}

int nb_engine_start_with_mgmt(nb_engine_t *engine, const char *setup_key) {
    if (!engine || !engine->config) {
        NB_LOG_ERROR("Invalid engine");
        return NB_ERROR_INVALID;
    }

    if (engine->running) {
        NB_LOG_WARN("Engine already running");
        return NB_SUCCESS;
    }

    int ret;

    NB_LOG_INFO("Starting NetBird engine with management...");

    /* Step 0: Create management client */
    if (!engine->mgmt_client) {
        engine->mgmt_client = mgmt_client_new(engine->config->management_url);
        if (!engine->mgmt_client) {
            NB_LOG_ERROR("Failed to create management client");
            return NB_ERROR_SYSTEM;
        }
    }

    /* Step 1: Register with management server */
    NB_LOG_INFO("Step 1: Registering with management server...");
    mgmt_config_t *mgmt_config = NULL;
    ret = mgmt_register(engine->mgmt_client, setup_key, &mgmt_config);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to register with management");
        return ret;
    }

    /* Step 2: Start basic engine (WireGuard interface + routes) */
    ret = nb_engine_start(engine);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to start engine");
        mgmt_config_free(mgmt_config);
        return ret;
    }

    /* Step 3: Add peers from management */
    NB_LOG_INFO("Step 3: Adding %d peer(s) from management...", mgmt_config->peer_count);
    for (int i = 0; i < mgmt_config->peer_count; i++) {
        mgmt_peer_t *mp = &mgmt_config->peers[i];

        /* Convert mgmt peer to nb_peer_info */
        nb_peer_info_t peer = {0};
        peer.public_key = mp->public_key;
        peer.endpoint = mp->endpoint;
        peer.keepalive = 25;  /* Default keepalive */

        /* Parse allowed_ips (single CIDR for stub) */
        peer.allowed_ips_count = 1;
        peer.allowed_ips = &mp->allowed_ips;

        ret = nb_engine_add_peer(engine, &peer);
        if (ret != NB_SUCCESS) {
            NB_LOG_WARN("Failed to add peer %s", mp->id);
        }
    }

    /* Step 4: Add routes from management */
    NB_LOG_INFO("Step 4: Adding %d route(s) from management...", mgmt_config->route_count);
    for (int i = 0; i < mgmt_config->route_count; i++) {
        route_config_t route = {
            .network = mgmt_config->routes[i],
            .device = engine->wg_iface->name,
            .metric = 100,
            .masquerade = 0
        };

        ret = route_add(engine->route_mgr, &route);
        if (ret != NB_SUCCESS) {
            NB_LOG_WARN("Failed to add route %s", mgmt_config->routes[i]);
        }
    }

    mgmt_config_free(mgmt_config);

    NB_LOG_INFO("========================================");
    NB_LOG_INFO("  NetBird connected successfully!");
    NB_LOG_INFO("========================================");

    return NB_SUCCESS;
}

int nb_engine_stop(nb_engine_t *engine) {
    if (!engine) {
        NB_LOG_ERROR("Invalid engine");
        return NB_ERROR_INVALID;
    }

    if (!engine->running) {
        NB_LOG_WARN("Engine not running");
        return NB_SUCCESS;
    }

    NB_LOG_INFO("Stopping NetBird engine...");

    /* Step 1: Remove all routes */
    if (engine->route_mgr) {
        NB_LOG_INFO("Step 1: Removing routes...");
        route_remove_all(engine->route_mgr);
        route_manager_free(engine->route_mgr);
        engine->route_mgr = NULL;
    }

    /* Step 2: Destroy WireGuard interface */
    if (engine->wg_iface) {
        NB_LOG_INFO("Step 2: Destroying WireGuard interface...");
        wg_iface_destroy(engine->wg_iface);
        wg_iface_free(engine->wg_iface);
        engine->wg_iface = NULL;
    }

    /* Step 3: Close management client */
    if (engine->mgmt_client) {
        NB_LOG_INFO("Step 3: Closing management client...");
        mgmt_client_free(engine->mgmt_client);
        engine->mgmt_client = NULL;
    }

    engine->running = 0;

    NB_LOG_INFO("NetBird engine stopped");
    return NB_SUCCESS;
}

int nb_engine_add_peer(nb_engine_t *engine, const nb_peer_info_t *peer) {
    if (!engine || !peer || !peer->public_key) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    if (!engine->running || !engine->wg_iface) {
        NB_LOG_ERROR("Engine not running");
        return NB_ERROR_INVALID;
    }

    /* Build allowed IPs string */
    char allowed_ips[1024] = {0};
    if (peer->allowed_ips && peer->allowed_ips_count > 0) {
        for (int i = 0; i < peer->allowed_ips_count; i++) {
            if (i > 0) strcat(allowed_ips, ",");
            strcat(allowed_ips, peer->allowed_ips[i]);
        }
    }

    NB_LOG_INFO("Adding peer: %s", peer->public_key);
    NB_LOG_INFO("  Allowed IPs: %s", allowed_ips[0] ? allowed_ips : "(none)");
    NB_LOG_INFO("  Endpoint:    %s", peer->endpoint ? peer->endpoint : "(none)");
    NB_LOG_INFO("  Keepalive:   %d", peer->keepalive);

    int ret = wg_iface_update_peer(
        engine->wg_iface,
        peer->public_key,
        allowed_ips[0] ? allowed_ips : NULL,
        peer->keepalive,
        peer->endpoint,
        NULL  /* no pre-shared key for now */
    );

    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to add peer");
        return ret;
    }

    NB_LOG_INFO("Peer added successfully");
    return NB_SUCCESS;
}

int nb_engine_remove_peer(nb_engine_t *engine, const char *public_key) {
    if (!engine || !public_key) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    if (!engine->running || !engine->wg_iface) {
        NB_LOG_ERROR("Engine not running");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Removing peer: %s", public_key);

    int ret = wg_iface_remove_peer(engine->wg_iface, public_key);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to remove peer");
        return ret;
    }

    NB_LOG_INFO("Peer removed successfully");
    return NB_SUCCESS;
}

void nb_engine_free(nb_engine_t *engine) {
    if (!engine) return;

    /* Note: Config is freed separately by caller if needed */
    free(engine);
}

void nb_peer_info_free(nb_peer_info_t *peer) {
    if (!peer) return;

    free(peer->public_key);
    free(peer->endpoint);
    nb_free_string_array(peer->allowed_ips, peer->allowed_ips_count);
    free(peer);
}
