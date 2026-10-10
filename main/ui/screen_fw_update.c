#include "screen_fw_update.h"
#include "display.h"
#include "rotary_encoder.h"
#include "ui_manager.h"
#include "app_fw_update_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"

static const char *TAG = "screen_fw_update";

/* ---------------- Configuration ---------------- */

#define FW_CHECK_TASK_STACK       6144
#define FW_CHECK_TASK_PRIORITY    5

#define FW_OTA_TASK_STACK         8192
#define FW_OTA_TASK_PRIORITY      5

/* ---------------- Screen state ---------------- */

typedef enum {
    FW_SCREEN_IDLE,
    FW_SCREEN_CHECKING,
    FW_SCREEN_UP_TO_DATE,
    FW_SCREEN_AVAILABLE,
    FW_SCREEN_NO_UPDATE,
    FW_SCREEN_ERROR,
    FW_SCREEN_CONFIRM,
    FW_SCREEN_INSTALLING
} fw_screen_state_t;

static volatile fw_screen_state_t s_state = FW_SCREEN_IDLE;

static bool s_select_yes = true;

static char s_installed_version[32] = {0};
static char s_latest_version[32] = {0};
static char s_download_url[512] = {0};

static TaskHandle_t s_check_task_handle = NULL;
static TaskHandle_t s_ota_task_handle = NULL;

/* ---------------- Function declarations ---------------- */

static esp_err_t get_installed_version(char *version, size_t version_size);

static int compare_versions(const char *a, const char *b);

static void firmware_check_task(void *arg);
static void firmware_ota_task(void *arg);

static void start_firmware_check_task(void);
static void start_firmware_ota_task(const char *url);

/* =========================================================
 * Read currently installed firmware version
 * ========================================================= */

static esp_err_t get_installed_version(char *version, size_t version_size){
    if (version == NULL || version_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *partition = esp_ota_get_running_partition();

    if (partition == NULL) {
        return ESP_FAIL;
    }

    esp_app_desc_t app_desc = {0};

    esp_err_t err = esp_ota_get_partition_description(partition, &app_desc);

    if (err != ESP_OK) {
        return err;
    }

    int written = snprintf(version, version_size, "%s", app_desc.version);

    if (written < 0 || (size_t)written >= version_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

/* =========================================================
 * Compare numeric versions: major.minor.patch
 *
 * Returns:
 *   1  -> a is newer
 *   0  -> versions are equal
 *  -1  -> b is newer
 *  -2  -> invalid version format
 * ========================================================= */

static int compare_versions(
    const char *a,
    const char *b)
{
    unsigned int a_major, a_minor, a_patch;
    unsigned int b_major, b_minor, b_patch;

    char extra;

    if (a == NULL || b == NULL) {
        return -2;
    }

    if (sscanf(a, "%u.%u.%u%c",
               &a_major, &a_minor, &a_patch,
               &extra) != 3 ||
        sscanf(b, "%u.%u.%u%c",
               &b_major, &b_minor, &b_patch,
               &extra) != 3) {
        return -2;
    }

    if (a_major != b_major) {
        return a_major > b_major ? 1 : -1;
    }

    if (a_minor != b_minor) {
        return a_minor > b_minor ? 1 : -1;
    }

    if (a_patch != b_patch) {
        return a_patch > b_patch ? 1 : -1;
    }

    return 0;
}

/* =========================================================
 * GitHub release-check task
 * ========================================================= */

static void firmware_check_task(void *arg)
{
    (void)arg;

    fw_release_info_t release = {0};
    char installed_version[32] = {0};

    esp_err_t err = fw_check_latest_release(&release);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Release check failed: %s",
                 esp_err_to_name(err));

        s_state = FW_SCREEN_ERROR;
        goto cleanup;
    }

    err = get_installed_version(
        installed_version,
        sizeof(installed_version));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read installed version");

        s_state = FW_SCREEN_ERROR;
        goto cleanup;
    }

    int result = compare_versions(
        release.version,
        installed_version);

    if (result == -2) {
        ESP_LOGE(TAG,
                 "Invalid version: installed=%s latest=%s",
                 installed_version,
                 release.version);

        s_state = FW_SCREEN_ERROR;
        goto cleanup;
    }

    /*
     * Copy the results before changing the state,
     * so the UI sees the updated information.
     */

    snprintf(s_installed_version,
             sizeof(s_installed_version),
             "%s", installed_version);

    snprintf(s_latest_version,
             sizeof(s_latest_version),
             "%s", release.version);

    snprintf(s_download_url,
             sizeof(s_download_url),
             "%s", release.bin_url);

    if (result > 0) {
        ESP_LOGI(TAG, "New firmware available: %s",
                 s_latest_version);

        s_state = FW_SCREEN_AVAILABLE;
    }
    else if (result == 0) {
        ESP_LOGI(TAG, "Firmware is up to date");

        s_state = FW_SCREEN_UP_TO_DATE;
    }
    else {
        ESP_LOGW(TAG,
                 "Installed firmware is newer than GitHub release");

        s_state = FW_SCREEN_NO_UPDATE;
    }

cleanup:
    s_check_task_handle = NULL;
    vTaskDelete(NULL);
}

/* =========================================================
 * Start release-check task
 * ========================================================= */

static void start_firmware_check_task(void)
{
    if (s_check_task_handle != NULL ||
        s_state == FW_SCREEN_CHECKING ||
        s_state == FW_SCREEN_INSTALLING) {
        return;
    }

    s_state = FW_SCREEN_CHECKING;

    BaseType_t ret = xTaskCreate(
        firmware_check_task,
        "fw_check",
        FW_CHECK_TASK_STACK,
        NULL,
        FW_CHECK_TASK_PRIORITY,
        &s_check_task_handle);

    if (ret != pdPASS) {
        s_check_task_handle = NULL;
        s_state = FW_SCREEN_ERROR;

        ESP_LOGE(TAG,
                 "Failed to create firmware check task");
    }
}

/* =========================================================
 * OTA task: download and install firmware
 * ========================================================= */

static void firmware_ota_task(void *arg){
    char *url = (char *)arg;

    if (url == NULL) {
        s_ota_task_handle = NULL;
        s_state = FW_SCREEN_ERROR;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Starting HTTPS OTA");
    ESP_LOGI(TAG, "Firmware URL: %s", url);

    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 8192,
        .buffer_size_tx = 8192,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t err = esp_https_ota(&ota_config);

    free(url);

    if (err == ESP_OK) {
        ESP_LOGI(TAG,
                 "OTA successful. Restarting device.");

        /*
         * esp_https_ota() has completed successfully.
         * Restart into the newly installed firmware.
         */
        esp_restart();

        /* Normally never reached. */
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGE(TAG, "OTA failed: %s",
             esp_err_to_name(err));

    s_ota_task_handle = NULL;
    s_state = FW_SCREEN_ERROR;

    vTaskDelete(NULL);
}

/* =========================================================
 * Start OTA task with a copied URL
 * ========================================================= */

static void start_firmware_ota_task(const char *url){
    if (url == NULL || url[0] == '\0' || s_ota_task_handle != NULL) {
        ESP_LOGE(TAG, "Invalid OTA URL or task already running");
        s_state = FW_SCREEN_ERROR;
        return;
    }

    size_t url_len = strlen(url) + 1;

    char *url_copy = malloc(url_len);

    if (url_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate OTA URL");

        s_state = FW_SCREEN_ERROR;
        return;
    }

    memcpy(url_copy, url, url_len);

    BaseType_t ret = xTaskCreate(firmware_ota_task, "fw_ota", FW_OTA_TASK_STACK, url_copy, FW_OTA_TASK_PRIORITY, &s_ota_task_handle);

    if (ret != pdPASS) {
        free(url_copy);

        s_ota_task_handle = NULL;
        s_state = FW_SCREEN_ERROR;

        ESP_LOGE(TAG, "Failed to create OTA task");
        return;
    }

    s_state = FW_SCREEN_INSTALLING;
}

/* =========================================================
 * Screen enter
 * ========================================================= */

void screen_fw_update_enter(void){
    esp_err_t err = get_installed_version(s_installed_version, sizeof(s_installed_version));

    if (err != ESP_OK) {
        snprintf(s_installed_version, sizeof(s_installed_version), "unknown");
    }

    s_latest_version[0] = '\0';
    s_download_url[0] = '\0';

    s_select_yes = true;
    s_state = FW_SCREEN_IDLE;
}

/* =========================================================
 * Encoder events
 *
 * Short press: check / continue / confirm
 * Rotate: select Yes or No
 * Long press: return to menu
 * ========================================================= */

void screen_fw_update_event(encoder_event_t evt){
    if (s_state == FW_SCREEN_INSTALLING) {
        return;
    }

    switch (evt) {

    case ENC_EVT_SHORT_PRESS:

        if (s_state == FW_SCREEN_IDLE ||
            s_state == FW_SCREEN_ERROR ||
            s_state == FW_SCREEN_UP_TO_DATE) {

            start_firmware_check_task();
        }
        else if (s_state == FW_SCREEN_AVAILABLE) {

            s_select_yes = true;
            s_state = FW_SCREEN_CONFIRM;
        }
        else if (s_state == FW_SCREEN_CONFIRM) {

            if (s_select_yes) {
                start_firmware_ota_task(s_download_url);
            }
            else {
                s_state = FW_SCREEN_IDLE;
            }
        }
        else if (s_state == FW_SCREEN_NO_UPDATE) {

            s_state = FW_SCREEN_IDLE;
        }

        break;

    case ENC_EVT_CW:
    case ENC_EVT_CCW:

        if (s_state == FW_SCREEN_CONFIRM) {
            s_select_yes = !s_select_yes;
        }

        break;

    case ENC_EVT_LONG_PRESS:

        if (s_state != FW_SCREEN_CHECKING) {
            ui_manager_goto(SCREEN_MENU);
        }

        break;

    default:
        break;
    }
}

/* =========================================================
 * Screen rendering
 * ========================================================= */

void screen_fw_update_tick(void){
    char buf[64];

    display_clear();

    switch (s_state) {

    case FW_SCREEN_IDLE:

        display_draw_text(2, 10, "Firmware Update");

        snprintf(buf, sizeof(buf), "Installed: %s",
                 s_installed_version);

        display_draw_text(2, 26, buf);
        display_draw_text(2, 44, "Press: Check update");

        break;

    case FW_SCREEN_CHECKING:

        display_draw_text(2, 10, "Firmware Update");
        display_draw_text(2, 30, "Checking GitHub...");

        break;

    case FW_SCREEN_UP_TO_DATE:

        display_draw_text(2, 10, "Firmware Update");
        display_draw_text(2, 30, "Firmware up to date");
        display_draw_text(2, 48, "Press: Check again");

        break;

    case FW_SCREEN_AVAILABLE:

        display_draw_text(2, 10, "Update Available");

        snprintf(buf, sizeof(buf), "Old: %s",
                 s_installed_version);

        display_draw_text(2, 27, buf);

        snprintf(buf, sizeof(buf), "New: %s",
                 s_latest_version);

        display_draw_text(2, 40, buf);
        display_draw_text(2, 58, "Press: Continue");

        break;

    case FW_SCREEN_CONFIRM:

        display_draw_text(2, 10, "Install update?");

        snprintf(buf, sizeof(buf), "Version: %s",
                 s_latest_version);

        display_draw_text(2, 27, buf);

        display_draw_text(
            2, 58,
            s_select_yes ? ">Yes     No" : " Yes    >No");

        break;

    case FW_SCREEN_INSTALLING:

        display_draw_text(2, 10, "Firmware Update");
        display_draw_text(2, 30, "Installing...");
        display_draw_text(2, 48, "Please wait");

        break;

    case FW_SCREEN_NO_UPDATE:

        display_draw_text(2, 10, "No newer release");
        display_draw_text(2, 30, "Installed is newer");
        display_draw_text(2, 50, "Press to return");

        break;

    case FW_SCREEN_ERROR:

        display_draw_text(2, 10, "Update failed");
        display_draw_text(2, 30, "Check Wi-Fi/API");
        display_draw_text(2, 50, "Press: Retry");

        break;

    default:
        break;
    }

    display_update();
}
