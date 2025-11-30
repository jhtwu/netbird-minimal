/**
 * test_engine.c - Test program for NetBird engine
 *
 * This tests the complete engine workflow:
 * - Load/create configuration
 * - Start engine (creates WireGuard interface + route manager)
 * - Add test peer
 * - Add routes
 * - Stop engine (cleanup)
 *
 * Usage: sudo ./test_engine
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "common.h"
#include "config.h"
#include "wg_iface.h"
#include "route.h"
#include "engine.h"

int main(void) {
    int ret;
    nb_config_t *cfg = NULL;
    nb_engine_t *engine = NULL;
    char *privkey = NULL;

    printf("\n");
    printf("================================================================================\n");
    printf("  NetBird Minimal C Client - Engine Integration Test\n");
    printf("================================================================================\n\n");

    /* Check if running as root */
    if (geteuid() != 0) {
        printf("ERROR: This test must be run as root (use sudo)\n");
        return 1;
    }

    /* Test 1: Create configuration */
    printf("[Test 1] Creating configuration...\n");
    ret = config_new_default(&cfg);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not create config\n");
        return 1;
    }
    free(cfg->wg_iface_name);
    char ifname[32];
    snprintf(ifname, sizeof(ifname), "wtnb-eng-%d", getpid() % 10000);
    cfg->wg_iface_name = strdup(ifname);

    /* Generate WireGuard key and set parameters */
    wg_generate_private_key(&privkey);
    cfg->wg_private_key = privkey;
    cfg->wg_address = strdup("203.0.113.252/32");
    cfg->wg_listen_port = 53003 + (getpid() % 500); /* avoid clashes */
    cfg->management_url = strdup("https://api.netbird.io:443");
    cfg->signal_url = strdup("https://signal.netbird.io:443");

    printf("  Interface:  %s\n", cfg->wg_iface_name);
    printf("  Address:    %s\n", cfg->wg_address);
    printf("  Port:       %d\n", cfg->wg_listen_port);
    printf("  Management: %s\n", cfg->management_url);
    printf("  SUCCESS: Configuration created\n\n");

    /* Test 2: Create engine */
    printf("[Test 2] Creating engine...\n");
    engine = nb_engine_new(cfg);
    if (!engine) {
        printf("  FAILED: Could not create engine\n");
        config_free(cfg);
        return 1;
    }
    printf("  SUCCESS: Engine created\n\n");

    /* Test 3: Start engine */
    printf("[Test 3] Starting engine...\n");
    ret = nb_engine_start(engine);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not start engine\n");
        nb_engine_free(engine);
        config_free(cfg);
        return 1;
    }
    printf("  SUCCESS: Engine started\n\n");

    /* Test 4: Add a test peer */
    printf("[Test 4] Adding test peer...\n");

    /* Generate a test peer (in real scenario, this comes from management server) */
    char *peer_privkey = NULL;
    char *peer_pubkey = NULL;
    wg_generate_private_key(&peer_privkey);
    wg_get_public_key(peer_privkey, &peer_pubkey);

    nb_peer_info_t peer = {0};
    peer.public_key = peer_pubkey;

    /* Allocate allowed IPs array */
    peer.allowed_ips_count = 2;
    peer.allowed_ips = calloc(peer.allowed_ips_count, sizeof(char*));
    peer.allowed_ips[0] = strdup("100.64.0.200/32");
    peer.allowed_ips[1] = strdup("10.0.0.0/24");

    peer.endpoint = strdup("203.0.113.10:51820");
    peer.keepalive = 25;

    ret = nb_engine_add_peer(engine, &peer);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not add peer\n");
    } else {
        printf("  SUCCESS: Peer added\n");
    }
    printf("\n");

    /* Test 5: Add routes for peer networks */
    printf("[Test 5] Adding routes for peer networks...\n");

    route_config_t route1 = {
        .network = "10.0.0.0/24",
        .device = engine->wg_iface->name,
        .metric = 100,
        .masquerade = 0
    };

    ret = route_add(engine->route_mgr, &route1);
    if (ret != NB_SUCCESS) {
        printf("  WARNING: Could not add route for 10.0.0.0/24\n");
    } else {
        printf("  Added route: 10.0.0.0/24\n");
    }

    route_config_t route2 = {
        .network = "100.64.0.200/32",
        .device = engine->wg_iface->name,
        .metric = 100,
        .masquerade = 0
    };

    ret = route_add(engine->route_mgr, &route2);
    if (ret != NB_SUCCESS) {
        printf("  WARNING: Could not add route for 100.64.0.200/32\n");
    } else {
        printf("  Added route: 100.64.0.200/32\n");
    }
    printf("  SUCCESS: Routes added\n\n");

    /* Test 6: Show WireGuard interface status */
    printf("[Test 6] WireGuard interface status:\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wg show %s", engine->wg_iface->name);
    printf("  Running: %s\n", cmd);
    system(cmd);
    printf("\n");

    /* Test 7: Show routing table */
    printf("[Test 7] Routing table for %s:\n", engine->wg_iface->name);
    snprintf(cmd, sizeof(cmd), "ip route show dev %s", engine->wg_iface->name);
    printf("  Running: %s\n", cmd);
    system(cmd);
    printf("\n");

    /* Test 8: Show interface details */
    printf("[Test 8] Interface details:\n");
    snprintf(cmd, sizeof(cmd), "ip addr show %s", engine->wg_iface->name);
    printf("  Running: %s\n", cmd);
    system(cmd);
    printf("\n");

    /* Test 9: Remove peer */
    printf("[Test 9] Removing peer...\n");
    ret = nb_engine_remove_peer(engine, peer_pubkey);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not remove peer\n");
    } else {
        printf("  SUCCESS: Peer removed\n");
    }
    printf("\n");

    /* Test 10: Stop engine */
    printf("[Test 10] Stopping engine...\n");
    ret = nb_engine_stop(engine);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not stop engine\n");
    } else {
        printf("  SUCCESS: Engine stopped\n");
    }
    printf("\n");

    /* Cleanup */
    nb_engine_free(engine);
    config_free(cfg);
    free(peer_privkey);
    free(peer_pubkey);
    nb_free_string_array(peer.allowed_ips, peer.allowed_ips_count);
    free(peer.endpoint);

    printf("================================================================================\n");
    printf("  All engine tests completed successfully!\n");
    printf("================================================================================\n\n");

    return 0;
}
