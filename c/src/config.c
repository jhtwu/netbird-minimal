/**
 * config.c - Configuration management with JSON support
 *
 * Reference: go/internal/config.go
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "config.h"
#include "common.h"
#include <cjson/cJSON.h>
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
    NB_LOG_INFO("Created default configuration");
    return NB_SUCCESS;
}

/* Helper: Get string from JSON object */
static char* json_get_string(cJSON *obj, const char *key) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsString(item)) {
        return nb_strdup(item->valuestring);
    }
    return NULL;
}

/* Helper: Get int from JSON object */
static int json_get_int(cJSON *obj, const char *key, int default_val) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return default_val;
}

/* Helper: Get string array from JSON object */
static int json_get_string_array(cJSON *obj, const char *key, char ***arr_out, int *count_out) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsArray(item)) {
        *arr_out = NULL;
        *count_out = 0;
        return NB_SUCCESS;
    }

    int count = cJSON_GetArraySize(item);
    if (count == 0) {
        *arr_out = NULL;
        *count_out = 0;
        return NB_SUCCESS;
    }

    char **arr = calloc(count, sizeof(char*));
    if (!arr) {
        return NB_ERROR_SYSTEM;
    }

    for (int i = 0; i < count; i++) {
        cJSON *elem = cJSON_GetArrayItem(item, i);
        if (elem && cJSON_IsString(elem)) {
            arr[i] = nb_strdup(elem->valuestring);
        }
    }

    *arr_out = arr;
    *count_out = count;
    return NB_SUCCESS;
}

int config_load(const char *path, nb_config_t **cfg_out) {
    if (!path || !cfg_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    /* Read file */
    FILE *f = fopen(path, "r");
    if (!f) {
        NB_LOG_WARN("Config file not found: %s", path);
        NB_LOG_INFO("Creating default configuration");
        return config_new_default(cfg_out);
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read content */
    char *content = malloc(fsize + 1);
    if (!content) {
        fclose(f);
        return NB_ERROR_SYSTEM;
    }

    size_t read_size = fread(content, 1, fsize, f);
    fclose(f);
    content[read_size] = '\0';

    /* Parse JSON */
    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root) {
        NB_LOG_ERROR("Failed to parse JSON config: %s", cJSON_GetErrorPtr());
        return NB_ERROR_INVALID;
    }

    /* Create config structure */
    nb_config_t *cfg = calloc(1, sizeof(nb_config_t));
    if (!cfg) {
        cJSON_Delete(root);
        return NB_ERROR_SYSTEM;
    }

    /* Load WireGuard config */
    cJSON *wg_config = cJSON_GetObjectItem(root, "WireGuardConfig");
    if (wg_config) {
        cfg->wg_private_key = json_get_string(wg_config, "PrivateKey");
        cfg->wg_address = json_get_string(wg_config, "Address");
        cfg->wg_listen_port = json_get_int(wg_config, "ListenPort", 51820);
        cfg->preshared_key = json_get_string(wg_config, "PreSharedKey");
    }

    /* Load server URLs */
    cfg->management_url = json_get_string(root, "ManagementURL");
    cfg->signal_url = json_get_string(root, "SignalURL");
    cfg->admin_url = json_get_string(root, "AdminURL");

    /* Load interface name */
    cfg->wg_iface_name = json_get_string(root, "WgIfaceName");
    if (!cfg->wg_iface_name) {
        cfg->wg_iface_name = nb_strdup("wt0");
    }

    /* Load peer ID */
    cfg->peer_id = json_get_string(root, "PeerID");

    /* Load NAT external IPs */
    json_get_string_array(root, "NATExternalIPs",
                         &cfg->nat_external_ips,
                         &cfg->nat_external_ips_count);

    /* Load DNS */
    cfg->custom_dns_addr = json_get_string(root, "CustomDNSAddress");

    /* Store config path */
    cfg->config_path = nb_strdup(path);

    cJSON_Delete(root);
    *cfg_out = cfg;

    NB_LOG_INFO("Loaded configuration from: %s", path);
    return NB_SUCCESS;
}

int config_save(const char *path, const nb_config_t *cfg) {
    if (!path || !cfg) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    /* Create JSON object */
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NB_ERROR_SYSTEM;
    }

    /* WireGuard config */
    cJSON *wg_config = cJSON_CreateObject();
    if (cfg->wg_private_key) {
        cJSON_AddStringToObject(wg_config, "PrivateKey", cfg->wg_private_key);
    }
    if (cfg->wg_address) {
        cJSON_AddStringToObject(wg_config, "Address", cfg->wg_address);
    }
    cJSON_AddNumberToObject(wg_config, "ListenPort", cfg->wg_listen_port);
    if (cfg->preshared_key) {
        cJSON_AddStringToObject(wg_config, "PreSharedKey", cfg->preshared_key);
    }
    cJSON_AddItemToObject(root, "WireGuardConfig", wg_config);

    /* Server URLs */
    if (cfg->management_url) {
        cJSON_AddStringToObject(root, "ManagementURL", cfg->management_url);
    }
    if (cfg->signal_url) {
        cJSON_AddStringToObject(root, "SignalURL", cfg->signal_url);
    }
    if (cfg->admin_url) {
        cJSON_AddStringToObject(root, "AdminURL", cfg->admin_url);
    }

    /* Interface name */
    if (cfg->wg_iface_name) {
        cJSON_AddStringToObject(root, "WgIfaceName", cfg->wg_iface_name);
    }

    /* Peer ID */
    if (cfg->peer_id) {
        cJSON_AddStringToObject(root, "PeerID", cfg->peer_id);
    }

    /* NAT external IPs */
    if (cfg->nat_external_ips && cfg->nat_external_ips_count > 0) {
        cJSON *ips_array = cJSON_CreateArray();
        for (int i = 0; i < cfg->nat_external_ips_count; i++) {
            if (cfg->nat_external_ips[i]) {
                cJSON_AddItemToArray(ips_array,
                                    cJSON_CreateString(cfg->nat_external_ips[i]));
            }
        }
        cJSON_AddItemToObject(root, "NATExternalIPs", ips_array);
    }

    /* DNS */
    if (cfg->custom_dns_addr) {
        cJSON_AddStringToObject(root, "CustomDNSAddress", cfg->custom_dns_addr);
    }

    /* Convert to string */
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_str) {
        return NB_ERROR_SYSTEM;
    }

    /* Create directory if needed */
    char *dir = strdup(path);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
        /* Ignore errors - directory might already exist */
    }
    free(dir);

    /* Write to file */
    FILE *f = fopen(path, "w");
    if (!f) {
        NB_LOG_ERROR("Failed to open config file for writing: %s", path);
        free(json_str);
        return NB_ERROR_SYSTEM;
    }

    fputs(json_str, f);
    fclose(f);
    free(json_str);

    NB_LOG_INFO("Saved configuration to: %s", path);
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
