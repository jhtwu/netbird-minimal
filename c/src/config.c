/**
 * config.c - Configuration management (Simplified version)
 *
 * Reference: go/internal/config.go (needs to be added from official NetBird)
 *
 * NOTE: This is a simplified version for Phase 1 testing.
 * Full JSON support will be added in Phase 2.
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "config.h"
#include "common.h"
#include <sys/stat.h>
#include <pwd.h>

const char* config_get_default_path(void) {
    static char path[512] = {0};

    if (path[0] != '\0') {
        return path;
    }

    /* Get home directory */
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (home) {
        snprintf(path, sizeof(path), "%s/.config/netbird/config.json", home);
    } else {
        snprintf(path, sizeof(path), "/etc/netbird/config.json");
    }

    return path;
}

int config_new_default(nb_config_t **cfg_out) {
    if (!cfg_out) {
        NB_LOG_ERROR("Invalid argument");
        return NB_ERROR_INVALID;
    }

    nb_config_t *cfg = calloc(1, sizeof(nb_config_t));
    if (!cfg) {
        NB_LOG_ERROR("calloc failed");
        return NB_ERROR_SYSTEM;
    }

    /* Set defaults */
    cfg->wg_iface_name = nb_strdup("wt0");
    cfg->wg_listen_port = 51820;
    cfg->config_path = nb_strdup(config_get_default_path());

    *cfg_out = cfg;
    return NB_SUCCESS;
}

int config_load(const char *path, nb_config_t **cfg_out) {
    if (!path || !cfg_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    /* TODO: Implement JSON loading in Phase 2 */
    /* For now, just return default config */

    NB_LOG_WARN("config_load: JSON support not yet implemented");
    NB_LOG_WARN("Using default configuration");

    return config_new_default(cfg_out);
}

int config_save(const char *path, const nb_config_t *cfg) {
    if (!path || !cfg) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    /* TODO: Implement JSON saving in Phase 2 */

    NB_LOG_WARN("config_save: JSON support not yet implemented");
    return NB_SUCCESS;
}

void config_free(nb_config_t *cfg) {
    if (!cfg) return;

    free(cfg->wg_private_key);
    free(cfg->wg_iface_name);
    free(cfg->wg_address);
    free(cfg->preshared_key);
    free(cfg->management_url);
    free(cfg->signal_url);
    free(cfg->admin_url);
    free(cfg->peer_id);
    free(cfg->custom_dns_addr);
    free(cfg->config_path);

    nb_free_string_array(cfg->nat_external_ips, cfg->nat_external_ips_count);

    free(cfg);
}
