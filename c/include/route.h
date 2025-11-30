/**
 * route.h - Route management for NetBird minimal C client
 *
 * Reference: go/internal/routemanager/systemops/systemops_linux.go
 *
 * This module manages system routing table entries.
 * Quick prototype version uses 'ip route' commands.
 * Production version should use netlink API.
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#ifndef NB_ROUTE_H
#define NB_ROUTE_H

/* Forward declaration */
typedef struct route_manager route_manager_t;

/**
 * Route configuration
 *
 * Reference: Go Route struct
 */
typedef struct {
    char *id;               /* Route identifier (optional) */
    char *network;          /* Network in CIDR notation, e.g., "10.0.0.0/8" */
    char *device;           /* Network device, e.g., "wt0" */
    int metric;             /* Route priority (lower = higher priority) */
    int masquerade;         /* 1 to enable NAT masquerading, 0 otherwise */
} route_config_t;

/**
 * Route manager structure
 */
struct route_manager {
    char *wg_device;        /* WireGuard device name */
    /* Could add route tracking here for cleanup */
};

/**
 * Create a new route manager
 *
 * @param wg_device WireGuard device name
 * @return Route manager instance, NULL on failure
 */
route_manager_t* route_manager_new(const char *wg_device);

/**
 * Add a route to the routing table
 *
 * Reference: Go AddRoute()
 *
 * Example: Add route for 10.0.0.0/8 via wt0
 *   route_add(mgr, &(route_config_t){
 *     .network = "10.0.0.0/8",
 *     .device = "wt0",
 *     .metric = 100,
 *     .masquerade = 0
 *   });
 *
 * @param mgr Route manager
 * @param route Route configuration
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int route_add(route_manager_t *mgr, const route_config_t *route);

/**
 * Remove a route from the routing table
 *
 * Reference: Go RemoveRoute()
 *
 * @param mgr Route manager
 * @param network Network to remove (CIDR notation)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int route_remove(route_manager_t *mgr, const char *network);

/**
 * Remove all routes for the WireGuard device
 *
 * @param mgr Route manager
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int route_remove_all(route_manager_t *mgr);

/**
 * Enable IP masquerading for a device
 *
 * This adds iptables rule for NAT.
 *
 * @param mgr Route manager
 * @param device Device to masquerade (usually WireGuard device)
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int route_enable_masquerade(route_manager_t *mgr, const char *device);

/**
 * Disable IP masquerading for a device
 *
 * @param mgr Route manager
 * @param device Device to un-masquerade
 * @return NB_SUCCESS on success, NB_ERROR_* on failure
 */
int route_disable_masquerade(route_manager_t *mgr, const char *device);

/**
 * Free route manager
 *
 * @param mgr Route manager to free
 */
void route_manager_free(route_manager_t *mgr);

#endif /* NB_ROUTE_H */
