/**
 * config.h - Configuration management for NetBird minimal C client
 *
 * Reference: Go config structure from client/internal/config.go
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#ifndef NB_CONFIG_H
#define NB_CONFIG_H

/**
 * NetBird client configuration
 *
 * This matches the Go Config struct for compatibility
 */
typedef struct {
    /* WireGuard configuration */
    char *wg_private_key;       /* WireGuard private key (base64) */
    char *wg_iface_name;        /* Interface name, e.g., "wt0" */
    char *wg_address;           /* WireGuard IP address, e.g., "100.64.0.5/16" */
    int wg_listen_port;         /* Listen port, default 51820 */
    char *preshared_key;        /* Optional pre-shared key */

    /* Server URLs */
    char *management_url;       /* Management server URL */
    char *signal_url;           /* Signal server URL */
    char *admin_url;            /* Admin panel URL (optional) */

    /* Peer information */
    char *peer_id;              /* Our peer ID (assigned by management) */

    /* NAT traversal */
    char **nat_external_ips;    /* External IP addresses for NAT */
    int nat_external_ips_count;

    /* DNS (optional for minimal version) */
    char *custom_dns_addr;

    /* Config file path */
    char *config_path;          /* Path to config.json */
} nb_config_t;

/**
 * Load configuration from JSON file
 *
 * @param path Path to config.json
 * @param cfg_out Output configuration (allocated by this function)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int config_load(const char *path, nb_config_t **cfg_out);

/**
 * Save configuration to JSON file
 *
 * @param path Path to config.json
 * @param cfg Configuration to save
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int config_save(const char *path, const nb_config_t *cfg);

/**
 * Create a new default configuration
 *
 * @param cfg_out Output configuration (allocated by this function)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int config_new_default(nb_config_t **cfg_out);

/**
 * Free configuration structure
 *
 * @param cfg Configuration to free
 */
void config_free(nb_config_t *cfg);

/**
 * Get default config path
 *
 * @return Default config path (~/.config/netbird/config.json)
 */
const char* config_get_default_path(void);

#endif /* NB_CONFIG_H */
