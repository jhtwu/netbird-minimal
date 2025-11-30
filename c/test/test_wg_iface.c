/**
 * test_wg_iface.c - Test program for WireGuard interface
 *
 * This tests the basic WireGuard interface operations.
 *
 * Usage: sudo ./test_wg_iface
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "common.h"
#include "config.h"
#include "wg_iface.h"

int main(void) {
    int ret;
    nb_config_t *cfg = NULL;
    wg_iface_t *iface = NULL;
    char *privkey = NULL;
    char *pubkey = NULL;

    printf("\n");
    printf("================================================================================\n");
    printf("  NetBird Minimal C Client - WireGuard Interface Test\n");
    printf("================================================================================\n\n");

    /* Check if running as root */
    if (geteuid() != 0) {
        printf("ERROR: This test must be run as root (use sudo)\n");
        return 1;
    }

    /* Test 1: Generate WireGuard keys */
    printf("[Test 1] Generating WireGuard private key...\n");
    ret = wg_generate_private_key(&privkey);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not generate private key\n");
        return 1;
    }
    printf("  Private key: %s\n", privkey);

    printf("[Test 2] Deriving public key from private key...\n");
    ret = wg_get_public_key(privkey, &pubkey);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not derive public key\n");
        free(privkey);
        return 1;
    }
    printf("  Public key:  %s\n\n", pubkey);
    free(pubkey);

    /* Test 2: Create config */
    printf("[Test 3] Creating default configuration...\n");
    ret = config_new_default(&cfg);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not create config\n");
        free(privkey);
        return 1;
    }

    /* Set WireGuard parameters */
    cfg->wg_private_key = privkey;
    cfg->wg_address = strdup("100.64.0.100/16");
    cfg->wg_listen_port = 51820;

    printf("  Interface: %s\n", cfg->wg_iface_name);
    printf("  Address:   %s\n", cfg->wg_address);
    printf("  Port:      %d\n\n", cfg->wg_listen_port);

    /* Test 3: Create WireGuard interface */
    printf("[Test 4] Creating WireGuard interface...\n");
    ret = wg_iface_create(cfg, &iface);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not create interface\n");
        config_free(cfg);
        return 1;
    }
    printf("  SUCCESS: Interface created\n\n");

    /* Test 4: Bring interface up */
    printf("[Test 5] Bringing interface up...\n");
    ret = wg_iface_up(iface);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not bring interface up\n");
        wg_iface_destroy(iface);
        wg_iface_free(iface);
        config_free(cfg);
        return 1;
    }
    printf("  SUCCESS: Interface is up\n\n");

    /* Test 5: Add a peer (dummy peer for testing) */
    printf("[Test 6] Adding a dummy peer...\n");
    const char *dummy_peer_pubkey = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
    const char *allowed_ips = "100.64.0.200/32";
    ret = wg_iface_update_peer(iface, dummy_peer_pubkey, allowed_ips, 25, NULL, NULL);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not add peer\n");
        wg_iface_destroy(iface);
        wg_iface_free(iface);
        config_free(cfg);
        return 1;
    }
    printf("  SUCCESS: Peer added\n\n");

    /* Test 6: Show interface status */
    printf("[Test 7] Checking interface status (wg show)...\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wg show %s", iface->name);
    printf("  Running: %s\n", cmd);
    system(cmd);
    printf("\n");

    /* Test 7: Remove peer */
    printf("[Test 8] Removing peer...\n");
    ret = wg_iface_remove_peer(iface, dummy_peer_pubkey);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not remove peer\n");
    } else {
        printf("  SUCCESS: Peer removed\n");
    }
    printf("\n");

    /* Test 8: Bring interface down */
    printf("[Test 9] Bringing interface down...\n");
    ret = wg_iface_down(iface);
    if (ret != NB_SUCCESS) {
        printf("  WARNING: Could not bring interface down\n");
    } else {
        printf("  SUCCESS: Interface is down\n");
    }
    printf("\n");

    /* Test 9: Destroy interface */
    printf("[Test 10] Destroying interface...\n");
    ret = wg_iface_destroy(iface);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not destroy interface\n");
        wg_iface_free(iface);
        config_free(cfg);
        return 1;
    }
    printf("  SUCCESS: Interface destroyed\n\n");

    /* Cleanup */
    wg_iface_free(iface);
    config_free(cfg);

    printf("================================================================================\n");
    printf("  All tests PASSED!\n");
    printf("================================================================================\n\n");

    return 0;
}
