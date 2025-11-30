/**
 * route.c - Route management (Quick prototype version)
 *
 * Reference: go/internal/routemanager/systemops/systemops_linux.go
 *
 * This is the QUICK PROTOTYPE version that uses 'ip route' commands.
 * Production version should use netlink API (libmnl).
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "route.h"
#include "common.h"

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

route_manager_t* route_manager_new(const char *wg_device) {
    if (!wg_device) {
        NB_LOG_ERROR("Invalid wg_device");
        return NULL;
    }

    route_manager_t *mgr = calloc(1, sizeof(route_manager_t));
    if (!mgr) {
        NB_LOG_ERROR("calloc failed");
        return NULL;
    }

    mgr->wg_device = nb_strdup(wg_device);
    if (!mgr->wg_device) {
        free(mgr);
        return NULL;
    }

    NB_LOG_INFO("Route manager created for device: %s", wg_device);
    return mgr;
}

int route_add(route_manager_t *mgr, const route_config_t *route) {
    if (!mgr || !route || !route->network) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    const char *device = route->device ? route->device : mgr->wg_device;
    int metric = route->metric > 0 ? route->metric : 100;

    NB_LOG_INFO("Adding route: %s via %s (metric: %d)", route->network, device, metric);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ip route add %s dev %s metric %d 2>/dev/null",
             route->network, device, metric);

    int ret = exec_cmd(cmd);
    if (ret != NB_SUCCESS) {
        /* Route might already exist */
        NB_LOG_WARN("Route add failed (may already exist): %s", route->network);
        /* Check if route exists */
        char check_cmd[512];
        snprintf(check_cmd, sizeof(check_cmd),
                "ip route show %s | grep -q '%s'", route->network, device);
        if (system(check_cmd) == 0) {
            NB_LOG_INFO("Route already exists, continuing");
            ret = NB_SUCCESS;
        }
    }

    /* Handle masquerading if requested */
    if (ret == NB_SUCCESS && route->masquerade) {
        ret = route_enable_masquerade(mgr, device);
    }

    return ret;
}

int route_remove(route_manager_t *mgr, const char *network) {
    if (!mgr || !network) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Removing route: %s", network);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ip route del %s 2>/dev/null", network);

    int ret = exec_cmd(cmd);
    if (ret != NB_SUCCESS) {
        NB_LOG_WARN("Route removal failed (may not exist): %s", network);
    }

    return ret;
}

int route_remove_all(route_manager_t *mgr) {
    if (!mgr || !mgr->wg_device) {
        NB_LOG_ERROR("Invalid route manager");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Removing all routes for device: %s", mgr->wg_device);

    /* Get all routes for this device and remove them */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
            "ip route show dev %s | while read route; do "
            "ip route del $route dev %s 2>/dev/null; done",
            mgr->wg_device, mgr->wg_device);

    return exec_cmd(cmd);
}

int route_enable_masquerade(route_manager_t *mgr, const char *device) {
    if (!mgr || !device) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Enabling masquerade for device: %s", device);

    char cmd[512];

    /* Add iptables NAT rule */
    snprintf(cmd, sizeof(cmd),
            "iptables -t nat -C POSTROUTING -o %s -j MASQUERADE 2>/dev/null || "
            "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE",
            device, device);

    int ret = exec_cmd(cmd);
    if (ret != NB_SUCCESS) {
        NB_LOG_ERROR("Failed to enable masquerade for %s", device);
        return ret;
    }

    /* Enable IP forwarding */
    ret = exec_cmd("sysctl -w net.ipv4.ip_forward=1 >/dev/null");
    if (ret != NB_SUCCESS) {
        NB_LOG_WARN("Failed to enable IP forwarding");
    }

    return NB_SUCCESS;
}

int route_disable_masquerade(route_manager_t *mgr, const char *device) {
    if (!mgr || !device) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    NB_LOG_INFO("Disabling masquerade for device: %s", device);

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
            "iptables -t nat -D POSTROUTING -o %s -j MASQUERADE 2>/dev/null",
            device);

    return exec_cmd(cmd);
}

void route_manager_free(route_manager_t *mgr) {
    if (!mgr) return;

    free(mgr->wg_device);
    free(mgr);
}
