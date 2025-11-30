/**
 * test_config.c - Test program for JSON configuration
 *
 * This tests JSON config load/save functionality.
 *
 * Usage: ./test_config
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "common.h"
#include "config.h"
#include "wg_iface.h"

int main(void) {
    int ret;
    nb_config_t *cfg1 = NULL;
    nb_config_t *cfg2 = NULL;
    char *privkey = NULL;
    char test_config_path[] = "/tmp/netbird_test_config.json";

    printf("\n");
    printf("================================================================================\n");
    printf("  NetBird Minimal C Client - JSON Configuration Test\n");
    printf("================================================================================\n\n");

    /* Test 1: Create config with data */
    printf("[Test 1] Creating configuration with sample data...\n");
    ret = config_new_default(&cfg1);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not create config\n");
        return 1;
    }

    /* Generate WireGuard key */
    wg_generate_private_key(&privkey);

    /* Set all fields */
    free(cfg1->wg_private_key);
    cfg1->wg_private_key = privkey;
    cfg1->wg_address = strdup("100.64.0.100/16");
    cfg1->wg_listen_port = 51820;
    cfg1->management_url = strdup("https://api.example.com:443");
    cfg1->signal_url = strdup("https://signal.example.com:443");
    cfg1->admin_url = strdup("https://admin.example.com");
    cfg1->peer_id = strdup("test-peer-12345");

    printf("  Interface:     %s\n", cfg1->wg_iface_name);
    printf("  Address:       %s\n", cfg1->wg_address);
    printf("  Port:          %d\n", cfg1->wg_listen_port);
    printf("  Management:    %s\n", cfg1->management_url);
    printf("  Signal:        %s\n", cfg1->signal_url);
    printf("  Peer ID:       %s\n", cfg1->peer_id);
    printf("  SUCCESS: Config created\n\n");

    /* Test 2: Save to JSON */
    printf("[Test 2] Saving config to JSON file...\n");
    printf("  Path: %s\n", test_config_path);
    ret = config_save(test_config_path, cfg1);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not save config\n");
        config_free(cfg1);
        return 1;
    }
    printf("  SUCCESS: Config saved\n\n");

    /* Test 3: Show JSON content */
    printf("[Test 3] JSON file content:\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cat %s", test_config_path);
    system(cmd);
    printf("\n");

    /* Test 4: Load from JSON */
    printf("[Test 4] Loading config from JSON file...\n");
    ret = config_load(test_config_path, &cfg2);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Could not load config\n");
        config_free(cfg1);
        return 1;
    }
    printf("  SUCCESS: Config loaded\n\n");

    /* Test 5: Verify loaded data */
    printf("[Test 5] Verifying loaded data...\n");
    int errors = 0;

    if (strcmp(cfg2->wg_iface_name, cfg1->wg_iface_name) != 0) {
        printf("  ERROR: Interface name mismatch\n");
        errors++;
    }
    if (strcmp(cfg2->wg_address, cfg1->wg_address) != 0) {
        printf("  ERROR: Address mismatch\n");
        errors++;
    }
    if (cfg2->wg_listen_port != cfg1->wg_listen_port) {
        printf("  ERROR: Port mismatch\n");
        errors++;
    }
    if (strcmp(cfg2->management_url, cfg1->management_url) != 0) {
        printf("  ERROR: Management URL mismatch\n");
        errors++;
    }
    if (strcmp(cfg2->signal_url, cfg1->signal_url) != 0) {
        printf("  ERROR: Signal URL mismatch\n");
        errors++;
    }
    if (strcmp(cfg2->peer_id, cfg1->peer_id) != 0) {
        printf("  ERROR: Peer ID mismatch\n");
        errors++;
    }

    if (errors == 0) {
        printf("  Interface:     %s ✓\n", cfg2->wg_iface_name);
        printf("  Address:       %s ✓\n", cfg2->wg_address);
        printf("  Port:          %d ✓\n", cfg2->wg_listen_port);
        printf("  Management:    %s ✓\n", cfg2->management_url);
        printf("  Signal:        %s ✓\n", cfg2->signal_url);
        printf("  Peer ID:       %s ✓\n", cfg2->peer_id);
        printf("  SUCCESS: All fields match!\n");
    } else {
        printf("  FAILED: %d mismatches\n", errors);
    }
    printf("\n");

    /* Test 6: Test non-existent config */
    printf("[Test 6] Loading non-existent config (should create default)...\n");
    nb_config_t *cfg3 = NULL;
    ret = config_load("/tmp/nonexistent_config.json", &cfg3);
    if (ret != NB_SUCCESS) {
        printf("  FAILED: Load failed\n");
    } else {
        printf("  SUCCESS: Created default config\n");
        printf("  Interface:     %s\n", cfg3->wg_iface_name);
        printf("  Port:          %d\n", cfg3->wg_listen_port);
        config_free(cfg3);
    }
    printf("\n");

    /* Cleanup */
    config_free(cfg1);
    config_free(cfg2);
    unlink(test_config_path);

    printf("================================================================================\n");
    if (errors == 0) {
        printf("  All tests PASSED!\n");
    } else {
        printf("  Some tests FAILED\n");
    }
    printf("================================================================================\n\n");

    return errors > 0 ? 1 : 0;
}
