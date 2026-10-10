#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    char version[32];
    char bin_url[512];
} fw_release_info_t;

esp_err_t fw_check_latest_release(fw_release_info_t *release);