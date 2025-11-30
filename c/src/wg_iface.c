/**
 * wg_iface.c - WireGuard interface management (Quick prototype version)
 *
 * Reference: go/iface/iface_new_linux.go, go/iface/device/wg_link_linux.go
 *
 * This is the QUICK PROTOTYPE version that uses shell commands.
 * Production version should use netlink API (libmnl).
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "wg_iface.h"
#include "common.h"
#include <sys/stat.h>

/* Helper: Execute shell command and check result */
static int exec_cmd(const char *cmd) {
    NB_LOG_DEBUG("Executing: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        NB_LOG_ERROR("Command failed (exit %d): %s", ret, cmd);
        return NB_ERROR_SYSTEM;
    }
    return NB_SUCCESS;
}

/* Helper: Write string to temporary file and return path */
static int write_temp_file(const char *content, char **path_out) {
    char template[] = "/tmp/nb-wg-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) {
        NB_LOG_ERROR("mkstemp failed: %s", strerror(errno));
        return NB_ERROR_SYSTEM;
    }

    ssize_t len = strlen(content);
    ssize_t written = write(fd, content, len);
    close(fd);

    if (written != len) {
        NB_LOG_ERROR("write failed: %s", strerror(errno));
        unlink(template);
        return NB_ERROR_SYSTEM;
    }

    *path_out = nb_strdup(template);
    return NB_SUCCESS;
}

int wg_iface_create(const nb_config_t *cfg, wg_iface_t **iface_out) {
    if (!cfg || !iface_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    if (!cfg->wg_iface_name || !cfg->wg_address || !cfg->wg_private_key) {
        NB_LOG_ERROR("Missing required config: iface_name, address, or private_key");
        return NB_ERROR_INVALID;
    }

    /* Allocate interface structure */
    wg_iface_t *iface = calloc(1, sizeof(wg_iface_t));
    if (!iface) {
        NB_LOG_ERROR("calloc failed");
        return NB_ERROR_SYSTEM;
    }

    iface->name = nb_strdup(cfg->wg_iface_name);
    iface->address = nb_strdup(cfg->wg_address);
    iface->private_key = nb_strdup(cfg->wg_private_key);
    iface->listen_port = cfg->wg_listen_port > 0 ? cfg->wg_listen_port : 51820;

    char cmd[1024];
    int ret;

    /* Step 1: Create WireGuard interface */
    NB_LOG_INFO("Creating WireGuard interface: %s", iface->name);
    snprintf(cmd, sizeof(cmd), "ip link add dev %s type wireguard 2>/dev/null", iface->name);
    ret = exec_cmd(cmd);
    if (ret != NB_SUCCESS) {
        /* Check if interface already exists */
        snprintf(cmd, sizeof(cmd), "ip link show %s >/dev/null 2>&1", iface->name);
        if (system(cmd) == 0) {
            NB_LOG_WARN("Interface %s already exists, using it", iface->name);
        } else {
            goto error;
        }
    }
    iface->created = 1;

    /* Step 2: Assign IP address */
    NB_LOG_INFO("Assigning IP address: %s", iface->address);
    snprintf(cmd, sizeof(cmd), "ip address add %s dev %s", iface->address, iface->name);
    ret = exec_cmd(cmd);
    if (ret != NB_SUCCESS) {
        /* Address might already be assigned */
        NB_LOG_WARN("Failed to assign address (may already exist)");
    }

    /* Step 3: Set private key and listen port */
    char *key_file = NULL;
    ret = write_temp_file(iface->private_key, &key_file);
    if (ret != NB_SUCCESS) {
        goto error;
    }

    NB_LOG_INFO("Configuring WireGuard (port: %d)", iface->listen_port);
    snprintf(cmd, sizeof(cmd), "wg set %s private-key %s listen-port %d",
             iface->name, key_file, iface->listen_port);
    ret = exec_cmd(cmd);
    unlink(key_file);
    free(key_file);

    if (ret != NB_SUCCESS) {
        goto error;
    }

    *iface_out = iface;
    NB_LOG_INFO("WireGuard interface %s created successfully", iface->name);
    return NB_SUCCESS;

error:
    if (iface->created) {
        /* Cleanup: remove interface */
        snprintf(cmd, sizeof(cmd), "ip link del dev %s", iface->name);
        system(cmd);
    }
    wg_iface_free(iface);
    return ret;
}

int wg_iface_up(wg_iface_t *iface) {
    if (!iface || !iface->name) {
        NB_LOG_ERROR("Invalid interface");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Bringing up interface: %s", iface->name);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link set dev %s up", iface->name);
    int ret = exec_cmd(cmd);
    if (ret == NB_SUCCESS) {
        iface->up = 1;
    }
    return ret;
}

int wg_iface_down(wg_iface_t *iface) {
    if (!iface || !iface->name) {
        NB_LOG_ERROR("Invalid interface");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Bringing down interface: %s", iface->name);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link set dev %s down", iface->name);
    int ret = exec_cmd(cmd);
    if (ret == NB_SUCCESS) {
        iface->up = 0;
    }
    return ret;
}

int wg_iface_update_peer(
    wg_iface_t *iface,
    const char *peer_pubkey,
    const char *allowed_ips,
    int persistent_keepalive,
    const char *endpoint,
    const char *preshared_key)
{
    if (!iface || !iface->name || !peer_pubkey) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Updating peer: %s (endpoint: %s)", peer_pubkey, endpoint ? endpoint : "none");

    char cmd[2048];
    int pos = 0;

    pos += snprintf(cmd + pos, sizeof(cmd) - pos, "wg set %s peer %s",
                    iface->name, peer_pubkey);

    if (allowed_ips) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " allowed-ips %s", allowed_ips);
    }

    if (endpoint) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " endpoint %s", endpoint);
    }

    if (persistent_keepalive > 0) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " persistent-keepalive %d",
                       persistent_keepalive);
    }

    if (preshared_key) {
        /* Write PSK to temp file */
        char *psk_file = NULL;
        int ret = write_temp_file(preshared_key, &psk_file);
        if (ret != NB_SUCCESS) {
            return ret;
        }

        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " preshared-key %s", psk_file);

        ret = exec_cmd(cmd);
        unlink(psk_file);
        free(psk_file);
        return ret;
    }

    return exec_cmd(cmd);
}

int wg_iface_remove_peer(wg_iface_t *iface, const char *peer_pubkey) {
    if (!iface || !iface->name || !peer_pubkey) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Removing peer: %s", peer_pubkey);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wg set %s peer %s remove", iface->name, peer_pubkey);
    return exec_cmd(cmd);
}

int wg_iface_destroy(wg_iface_t *iface) {
    if (!iface || !iface->name) {
        NB_LOG_ERROR("Invalid interface");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Destroying interface: %s", iface->name);

    char cmd[256];

    /* Bring down first */
    if (iface->up) {
        wg_iface_down(iface);
    }

    /* Delete interface */
    snprintf(cmd, sizeof(cmd), "ip link del dev %s", iface->name);
    int ret = exec_cmd(cmd);

    if (ret == NB_SUCCESS) {
        iface->created = 0;
    }

    return ret;
}

void wg_iface_free(wg_iface_t *iface) {
    if (!iface) return;

    free(iface->name);
    free(iface->address);
    free(iface->private_key);
    free(iface);
}

int wg_get_public_key(const char *private_key, char **public_key_out) {
    if (!private_key || !public_key_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    /* Write private key to temp file */
    char *priv_file = NULL;
    int ret = write_temp_file(private_key, &priv_file);
    if (ret != NB_SUCCESS) {
        return ret;
    }

    /* Generate public key */
    char cmd[512];
    char pub_file[] = "/tmp/nb-wg-pub-XXXXXX";
    int fd = mkstemp(pub_file);
    if (fd < 0) {
        NB_LOG_ERROR("mkstemp failed: %s", strerror(errno));
        unlink(priv_file);
        free(priv_file);
        return NB_ERROR_SYSTEM;
    }
    close(fd);

    snprintf(cmd, sizeof(cmd), "wg pubkey < %s > %s", priv_file, pub_file);
    ret = exec_cmd(cmd);
    unlink(priv_file);
    free(priv_file);

    if (ret != NB_SUCCESS) {
        unlink(pub_file);
        return ret;
    }

    /* Read public key */
    FILE *f = fopen(pub_file, "r");
    if (!f) {
        NB_LOG_ERROR("fopen failed: %s", strerror(errno));
        unlink(pub_file);
        return NB_ERROR_SYSTEM;
    }

    char pubkey[256];
    if (!fgets(pubkey, sizeof(pubkey), f)) {
        NB_LOG_ERROR("fgets failed");
        fclose(f);
        unlink(pub_file);
        return NB_ERROR_SYSTEM;
    }
    fclose(f);
    unlink(pub_file);

    /* Remove trailing newline */
    size_t len = strlen(pubkey);
    if (len > 0 && pubkey[len-1] == '\n') {
        pubkey[len-1] = '\0';
    }

    *public_key_out = nb_strdup(pubkey);
    return NB_SUCCESS;
}

int wg_generate_private_key(char **private_key_out) {
    if (!private_key_out) {
        NB_LOG_ERROR("Invalid argument");
        return NB_ERROR_INVALID;
    }

    char priv_file[] = "/tmp/nb-wg-genkey-XXXXXX";
    int fd = mkstemp(priv_file);
    if (fd < 0) {
        NB_LOG_ERROR("mkstemp failed: %s", strerror(errno));
        return NB_ERROR_SYSTEM;
    }
    close(fd);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wg genkey > %s", priv_file);
    int ret = exec_cmd(cmd);
    if (ret != NB_SUCCESS) {
        unlink(priv_file);
        return ret;
    }

    /* Read private key */
    FILE *f = fopen(priv_file, "r");
    if (!f) {
        NB_LOG_ERROR("fopen failed: %s", strerror(errno));
        unlink(priv_file);
        return NB_ERROR_SYSTEM;
    }

    char privkey[256];
    if (!fgets(privkey, sizeof(privkey), f)) {
        NB_LOG_ERROR("fgets failed");
        fclose(f);
        unlink(priv_file);
        return NB_ERROR_SYSTEM;
    }
    fclose(f);
    unlink(priv_file);

    /* Remove trailing newline */
    size_t len = strlen(privkey);
    if (len > 0 && privkey[len-1] == '\n') {
        privkey[len-1] = '\0';
    }

    *private_key_out = nb_strdup(privkey);
    NB_LOG_INFO("Generated new WireGuard private key");
    return NB_SUCCESS;
}
