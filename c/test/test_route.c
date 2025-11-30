/**
 * test_route.c - Test program for route management
 *
 * This tests route addition and removal.
 *
 * Usage: sudo ./test_route
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "common.h"
#include "config.h"
#include "wg_iface.h"
#include "route.h"

int main(void) {
    int ret;
    nb_config_t *cfg = NULL;
    wg_iface_t *iface = NULL;
    route_manager_t *route_mgr = NULL;
    char *privkey = NULL;

    printf("\n");
    printf("================================================================================\n");
    printf("  NetBird Minimal C Client - Route Management Test\n");
    printf("================================================================================\n\n");

    /* Check if running as root */
    if (geteuid() != 0) {
        printf("ERROR: This test must be run as root (use sudo)\n");
        return 1;
    }

    /* Setup: Create a WireGuard interface first */
    printf("[Setup] Creating WireGuard interface for routing tests...\n");

    wg_generate_private_key(&privkey);
    config_new_default(&cfg);
    cfg->wg_private_key = privkey;
    cfg->wg_address = strdup("100.64.0.100/16");

    ret = wg_iface_create(cfg, &iface);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not create interface\n");
        config_free(cfg);
        return 1;
    }

    ret = wg_iface_up(iface);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not bring interface up\n");
        wg_iface_destroy(iface);
        wg_iface_free(iface);
        config_free(cfg);
        return 1;
    }
    printf("  Interface %s is ready\n\n", iface->name);

    /* Test 1: Create route manager */
    printf("[Test 1] Creating route manager...\n");
    route_mgr = route_manager_new(iface->name);
    if (!route_mgr) {
        printf("  FAILED: Could not create route manager\n");
        goto cleanup;
    }
    printf("  SUCCESS: Route manager created\n\n");

    /* Test 2: Add a route */
    printf("[Test 2] Adding route for 10.0.0.0/24...\n");
    route_config_t route1 = {
        .network = "10.0.0.0/24",
        .device = iface->name,
        .metric = 100,
        .masquerade = 0
    };
    ret = route_add(route_mgr, &route1);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not add route\n");
    } else {
        printf("  SUCCESS: Route added\n");
    }
    printf("\n");

    /* Test 3: Add another route */
    printf("[Test 3] Adding route for 10.1.0.0/16...\n");
    route_config_t route2 = {
        .network = "10.1.0.0/16",
        .device = iface->name,
        .metric = 150,
        .masquerade = 0
    };
    ret = route_add(route_mgr, &route2);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not add route\n");
    } else {
        printf("  SUCCESS: Route added\n");
    }
    printf("\n");

    /* Test 4: Show routing table */
    printf("[Test 4] Checking routing table...\n");
    printf("  Running: ip route show dev %s\n", iface->name);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip route show dev %s", iface->name);
    system(cmd);
    printf("\n");

    /* Test 5: Remove one route */
    printf("[Test 5] Removing route 10.0.0.0/24...\n");
    ret = route_remove(route_mgr, "10.0.0.0/24");
    if (ret != NB_SUCCESS) {
        printf("  WARNING: Route removal reported failure\n");
    } else {
        printf("  SUCCESS: Route removed\n");
    }
    printf("\n");

    /* Test 6: Show routing table again */
    printf("[Test 6] Checking routing table after removal...\n");
    printf("  Running: ip route show dev %s\n", iface->name);
    system(cmd);
    printf("\n");

    /* Test 7: Remove all routes */
    printf("[Test 7] Removing all routes for device...\n");
    ret = route_remove_all(route_mgr);
    if (ret != NB_SUCCESS) {
        printf("  WARNING: Remove all reported issues\n");
    } else {
        printf("  SUCCESS: All routes removed\n");
    }
    printf("\n");

    printf("================================================================================\n");
    printf("  All tests completed!\n");
    printf("================================================================================\n\n");

cleanup:
    /* Cleanup */
    if (route_mgr) route_manager_free(route_mgr);
    if (iface) {
        wg_iface_destroy(iface);
        wg_iface_free(iface);
    }
    if (cfg) config_free(cfg);

    return 0;
}
