/**
 * peers_file.c - Peers JSON file reader implementation
 *
 * Author: Claude
 * Date: 2025-12-01
 */

#include "peers_file.h"
#include "common.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int peers_file_load(const char *path, peers_file_t **peers_out) {
    if (!path || !peers_out) {
        NB_LOG_ERROR("Invalid arguments");
        return NB_ERROR_INVALID;
    }

    /* Read file */
    FILE *fp = fopen(path, "r");
    if (!fp) {
        NB_LOG_ERROR("Failed to open %s", path);
        return NB_ERROR_NOTFOUND;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *json_str = malloc(size + 1);
    if (!json_str) {
        fclose(fp);
        return NB_ERROR_SYSTEM;
    }

    fread(json_str, 1, size, fp);
    json_str[size] = '\0';
    fclose(fp);

    /* Parse JSON */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        NB_LOG_ERROR("Failed to parse JSON from %s", path);
        return NB_ERROR_INVALID;
    }

    /* Allocate peers structure */
    peers_file_t *peers = calloc(1, sizeof(peers_file_t));
    if (!peers) {
        cJSON_Delete(root);
        return NB_ERROR_SYSTEM;
    }

    /* Get updatedAt */
    cJSON *updated_at = cJSON_GetObjectItem(root, "updatedAt");
    if (updated_at && cJSON_IsString(updated_at)) {
        peers->updated_at = strdup(updated_at->valuestring);
    }

    /* Get peers array */
    cJSON *peers_array = cJSON_GetObjectItem(root, "peers");
    if (!peers_array || !cJSON_IsArray(peers_array)) {
        NB_LOG_WARN("No peers array in %s", path);
        peers->peer_count = 0;
        peers->peers = NULL;
        *peers_out = peers;
        cJSON_Delete(root);
        return NB_SUCCESS;
    }

    int count = cJSON_GetArraySize(peers_array);
    if (count == 0) {
        peers->peer_count = 0;
        peers->peers = NULL;
        *peers_out = peers;
        cJSON_Delete(root);
        return NB_SUCCESS;
    }

    /* Allocate peers array */
    peers->peers = calloc(count, sizeof(peers_file_peer_t));
    if (!peers->peers) {
        free(peers);
        cJSON_Delete(root);
        return NB_ERROR_SYSTEM;
    }

    /* Parse each peer */
    for (int i = 0; i < count; i++) {
        cJSON *peer_json = cJSON_GetArrayItem(peers_array, i);
        if (!peer_json) continue;

        peers_file_peer_t *peer = &peers->peers[i];

        /* id */
        cJSON *id = cJSON_GetObjectItem(peer_json, "id");
        if (id && cJSON_IsString(id)) {
            peer->id = strdup(id->valuestring);
        }

        /* publicKey */
        cJSON *pubkey = cJSON_GetObjectItem(peer_json, "publicKey");
        if (pubkey && cJSON_IsString(pubkey)) {
            peer->public_key = strdup(pubkey->valuestring);
        }

        /* endpoint */
        cJSON *endpoint = cJSON_GetObjectItem(peer_json, "endpoint");
        if (endpoint && cJSON_IsString(endpoint)) {
            peer->endpoint = strdup(endpoint->valuestring);
        }

        /* keepalive */
        cJSON *keepalive = cJSON_GetObjectItem(peer_json, "keepalive");
        if (keepalive && cJSON_IsNumber(keepalive)) {
            peer->keepalive = keepalive->valueint;
        }

        /* allowedIPs */
        cJSON *allowed_ips = cJSON_GetObjectItem(peer_json, "allowedIPs");
        if (allowed_ips && cJSON_IsArray(allowed_ips)) {
            int ip_count = cJSON_GetArraySize(allowed_ips);
            peer->allowed_ips_count = ip_count;
            peer->allowed_ips = calloc(ip_count, sizeof(char*));

            for (int j = 0; j < ip_count; j++) {
                cJSON *ip = cJSON_GetArrayItem(allowed_ips, j);
                if (ip && cJSON_IsString(ip)) {
                    peer->allowed_ips[j] = strdup(ip->valuestring);
                }
            }
        }

        peers->peer_count++;
    }

    cJSON_Delete(root);
    *peers_out = peers;

    NB_LOG_INFO("Loaded %d peer(s) from %s", peers->peer_count, path);
    return NB_SUCCESS;
}

void peers_file_free(peers_file_t *peers) {
    if (!peers) return;

    for (int i = 0; i < peers->peer_count; i++) {
        free(peers->peers[i].id);
        free(peers->peers[i].public_key);
        free(peers->peers[i].endpoint);
        nb_free_string_array(peers->peers[i].allowed_ips, peers->peers[i].allowed_ips_count);
    }

    free(peers->peers);
    free(peers->updated_at);
    free(peers);
}
