/**
 * main.c - NetBird minimal C client CLI
 *
 * Simple command-line interface for NetBird client.
 *
 * Usage:
 *   netbird-client up              - Start NetBird
 *   netbird-client down            - Stop NetBird
 *   netbird-client status          - Show status
 *   netbird-client add-peer <key>  - Add peer manually
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "common.h"
#include "config.h"
#include "wg_iface.h"
#include "route.h"
#include "engine.h"
#include <signal.h>

#define DEFAULT_CONFIG_PATH "/etc/netbird/config.json"

/* Global engine pointer for signal handling */
static nb_engine_t *g_engine = NULL;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n");
        NB_LOG_INFO("Received signal %d, shutting down...", sig);
        if (g_engine) {
            nb_engine_stop(g_engine);
            nb_engine_free(g_engine);
            g_engine = NULL;
        }
        exit(0);
    }
}

void print_usage(const char *prog) {
    printf("\n");
    printf("NetBird Minimal C Client\n");
    printf("=========================\n\n");
    printf("Usage:\n");
    printf("  %s [-c CONFIG] up              - Start NetBird client\n", prog);
    printf("  %s [-c CONFIG] down            - Stop NetBird client\n", prog);
    printf("  %s [-c CONFIG] status          - Show WireGuard status\n", prog);
    printf("  %s [-c CONFIG] add-peer <key> <endpoint> <allowed-ips>\n", prog);
    printf("                                     - Add peer manually\n");
    printf("  %s --help                      - Show this help\n\n", prog);
    printf("Options:\n");
    printf("  -c CONFIG   - Use custom config file (default: %s)\n\n", DEFAULT_CONFIG_PATH);
    printf("Examples:\n");
    printf("  sudo %s up\n", prog);
    printf("  sudo %s -c /tmp/test.json up\n", prog);
    printf("  sudo %s add-peer ABC...XYZ= 1.2.3.4:51820 10.0.0.0/24\n", prog);
    printf("  sudo %s status\n", prog);
    printf("  sudo %s down\n\n", prog);
}

int cmd_up(const char *config_path) {
    int ret;
    nb_config_t *cfg = NULL;

    NB_LOG_INFO("Starting NetBird client...");

    /* Load configuration */
    ret = config_load(config_path, &cfg);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to load configuration from %s", config_path);
        return ret;
    }

    /* Validate critical config */
    if (!cfg->wg_private_key) {
        NB_LOG_ERROR("No WireGuard private key in config. Please configure first.");
        config_free(cfg);
        return NB_ERROR_INVALID;
    }

    if (!cfg->wg_address) {
        NB_LOG_ERROR("No WireGuard address in config. Please configure first.");
        config_free(cfg);
        return NB_ERROR_INVALID;
    }

    /* Create engine */
    g_engine = nb_engine_new(cfg);
    if (!g_engine) {
        NB_LOG_ERROR("Failed to create engine");
        config_free(cfg);
        return NB_ERROR_SYSTEM;
    }

    /* Start engine */
    ret = nb_engine_start(g_engine);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to start engine");
        nb_engine_free(g_engine);
        config_free(cfg);
        g_engine = NULL;
        return ret;
    }

    NB_LOG_INFO("NetBird client is running. Press Ctrl+C to stop.");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Keep running */
    while (1) {
        sleep(1);
    }

    return NB_SUCCESS;
}

int cmd_down(const char *config_path) {
    nb_config_t *cfg = NULL;
    int ret;

    NB_LOG_INFO("Stopping NetBird client...");

    /* Ensure config exists to avoid deleting unintended interfaces */
    if (access(config_path, F_OK) != 0) {
        NB_LOG_ERROR("Config file not found: %s", config_path);
        return NB_ERROR_NOTFOUND;
    }

    /* Load config to get interface name */
    ret = config_load(config_path, &cfg);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to load configuration");
        return ret;
    }

    /* Try to find and destroy existing interface */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link show %s >/dev/null 2>&1", cfg->wg_iface_name);
    if (system(cmd) == 0) {
        /* Interface exists, destroy it */
        snprintf(cmd, sizeof(cmd), "ip link del %s", cfg->wg_iface_name);
        system(cmd);
        NB_LOG_INFO("Interface %s removed", cfg->wg_iface_name);
    } else {
        NB_LOG_WARN("Interface %s not found", cfg->wg_iface_name);
    }

    config_free(cfg);
    NB_LOG_INFO("NetBird client stopped");
    return NB_SUCCESS;
}

int cmd_status(const char *config_path) {
    nb_config_t *cfg = NULL;
    int ret;

    if (access(config_path, F_OK) != 0) {
        NB_LOG_ERROR("Config file not found: %s", config_path);
        return NB_ERROR_NOTFOUND;
    }

    ret = config_load(config_path, &cfg);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to load configuration");
        return ret;
    }

    printf("\n");
    printf("================================================================================\n");
    printf("  NetBird Client Status\n");
    printf("================================================================================\n\n");

    /* Check if interface exists */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link show %s >/dev/null 2>&1", cfg->wg_iface_name);
    if (system(cmd) != 0) {
        printf("Status: NOT RUNNING\n");
        printf("Interface %s does not exist\n\n", cfg->wg_iface_name);
        config_free(cfg);
        return NB_ERROR_INVALID;
    }

    printf("Status: RUNNING\n");
    printf("Interface: %s\n\n", cfg->wg_iface_name);

    /* Show WireGuard status */
    printf("WireGuard Status:\n");
    printf("--------------------------------------------------------------------------------\n");
    snprintf(cmd, sizeof(cmd), "wg show %s", cfg->wg_iface_name);
    system(cmd);
    printf("\n");

    /* Show routes */
    printf("Routes:\n");
    printf("--------------------------------------------------------------------------------\n");
    snprintf(cmd, sizeof(cmd), "ip route show dev %s", cfg->wg_iface_name);
    system(cmd);
    printf("\n");

    /* Show interface details */
    printf("Interface Details:\n");
    printf("--------------------------------------------------------------------------------\n");
    snprintf(cmd, sizeof(cmd), "ip addr show %s", cfg->wg_iface_name);
    system(cmd);
    printf("\n");

    printf("================================================================================\n\n");

    config_free(cfg);
    return NB_SUCCESS;
}

int cmd_add_peer(const char *config_path, const char *pubkey,
                 const char *endpoint, const char *allowed_ips) {
    nb_config_t *cfg = NULL;
    wg_iface_t iface = {0};
    int ret;

    NB_LOG_INFO("Adding peer: %s", pubkey);

    if (access(config_path, F_OK) != 0) {
        NB_LOG_ERROR("Config file not found: %s", config_path);
        return NB_ERROR_NOTFOUND;
    }

    ret = config_load(config_path, &cfg);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to load configuration");
        return ret;
    }

    /* Setup minimal iface structure */
    iface.name = cfg->wg_iface_name;

    /* Add peer */
    ret = wg_iface_update_peer(&iface, pubkey, allowed_ips, 25, endpoint, NULL);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to add peer");
        config_free(cfg);
        return ret;
    }

    NB_LOG_INFO("Peer added successfully");
    config_free(cfg);
    return NB_SUCCESS;
}

int main(int argc, char *argv[]) {
    const char *config_path = DEFAULT_CONFIG_PATH;
    int arg_idx = 1;

    /* Parse command */
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse -c option */
    if (strcmp(argv[arg_idx], "-c") == 0) {
        if (argc < arg_idx + 2) {
            fprintf(stderr, "ERROR: -c requires a config file path\n");
            return 1;
        }
        config_path = argv[arg_idx + 1];
        arg_idx += 2;
    }

    if (arg_idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[arg_idx];

    /* Allow --help without root */
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    /* Check root privileges for other commands */
    if (geteuid() != 0) {
        fprintf(stderr, "ERROR: This program must be run as root (use sudo)\n");
        return 1;
    }

    if (strcmp(cmd, "up") == 0) {
        return cmd_up(config_path);
    }
    else if (strcmp(cmd, "down") == 0) {
        return cmd_down(config_path);
    }
    else if (strcmp(cmd, "status") == 0) {
        return cmd_status(config_path);
    }
    else if (strcmp(cmd, "add-peer") == 0) {
        if (argc < arg_idx + 4) {
            fprintf(stderr, "ERROR: add-peer requires <pubkey> <endpoint> <allowed-ips>\n");
            print_usage(argv[0]);
            return 1;
        }
        return cmd_add_peer(config_path, argv[arg_idx + 1], argv[arg_idx + 2], argv[arg_idx + 3]);
    }
    else {
        fprintf(stderr, "ERROR: Unknown command '%s'\n", cmd);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
