/**
 * peers_file.h - Peers JSON file reader (for hybrid architecture)
 *
 * Reads peers.json written by Go helper daemon.
 *
 * Author: Claude
 * Date: 2025-12-01
 */

#ifndef PEERS_FILE_H
#define PEERS_FILE_H

#include "common.h"

/* Peer information from JSON file */
typedef struct {
    char *id;
    char *public_key;
    char *endpoint;
    char **allowed_ips;      /* Array of CIDR strings */
    int allowed_ips_count;
    int keepalive;
} peers_file_peer_t;

/* Peers file structure */
typedef struct {
    peers_file_peer_t *peers;
    int peer_count;
    char *updated_at;
} peers_file_t;

/**
 * Load peers from JSON file
 *
 * @param path Path to peers.json
 * @param peers_out Output: peers structure (caller must free with peers_file_free)
 * @return NB_SUCCESS on success, error code on failure
 */
int peers_file_load(const char *path, peers_file_t **peers_out);

/**
 * Free peers file structure
 */
void peers_file_free(peers_file_t *peers);

#endif /* PEERS_FILE_H */
