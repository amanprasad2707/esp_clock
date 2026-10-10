#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    char version[32];
    char bin_url[512];
} fw_release_info_t;

esp_err_t fw_check_latest_release(fw_release_info_t *release);
esp_err_t get_installed_version(char *version, size_t version_size);
int compare_versions(const char *a, const char *b);