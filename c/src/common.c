/**
 * common.c - Common utilities
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#include "common.h"

char* nb_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) {
        NB_LOG_ERROR("strdup failed: %s", strerror(errno));
    }
    return dup;
}

int nb_str_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int nb_str_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

void nb_free_string_array(char **arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}
