/**
 * common.h - Common definitions for NetBird minimal C client
 *
 * Author: Claude
 * Date: 2025-11-30
 */

#ifndef NB_COMMON_H
#define NB_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Return codes */
#define NB_SUCCESS          0
#define NB_ERROR           -1
#define NB_ERROR_INVALID   -2
#define NB_ERROR_NOTFOUND  -3
#define NB_ERROR_EXISTS    -4
#define NB_ERROR_SYSTEM    -5
#define NB_ERROR_TIMEOUT   -6

/* Logging macros */
#define NB_LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define NB_LOG_WARN(fmt, ...) \
    fprintf(stderr, "[WARN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define NB_LOG_INFO(fmt, ...) \
    fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)

#define NB_LOG_DEBUG(fmt, ...) \
    fprintf(stdout, "[DEBUG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

/* String utilities */
char* nb_strdup(const char *s);
int nb_str_starts_with(const char *str, const char *prefix);
int nb_str_ends_with(const char *str, const char *suffix);

/* Memory utilities */
void nb_free_string_array(char **arr, int count);

#endif /* NB_COMMON_H */
