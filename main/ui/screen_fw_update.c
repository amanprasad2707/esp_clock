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
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    FW_SCREEN_INSTALLING,
    FW_SCREEN_OTA_DONE,
} fw_screen_state_t;

static volatile fw_screen_state_t s_state = FW_SCREEN_IDLE;

static bool s_select_yes = true;

static volatile int s_restart_countdown = -1;

static char s_installed_version[32] = {0};
static char s_latest_version[32] = {0};
static char s_download_url[512] = {0};

static TaskHandle_t s_check_task_handle = NULL;
static TaskHandle_t s_ota_task_handle = NULL;

/* ---------------- Function declarations ---------------- */

static void firmware_check_task(void *arg);
static void firmware_ota_task(void *arg);

static void start_firmware_check_task(void);
static void start_firmware_ota_task(const char *url);


/* =========================================================
 * GitHub release-check task
 * ========================================================= */

static void firmware_check_task(void *arg){
    (void)arg;

    fw_release_info_t release = {0};
    char installed_version[32] = {0};

    esp_err_t err = fw_check_latest_release(&release);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Release check failed: %s", esp_err_to_name(err));

        s_state = FW_SCREEN_ERROR;
        goto cleanup;
    }

    err = get_installed_version(installed_version, sizeof(installed_version));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read installed version");

        s_state = FW_SCREEN_ERROR;
        goto cleanup;
    }

    int result = compare_versions(release.version, installed_version);

    if (result == -2) {
        ESP_LOGE(TAG, "Invalid version: installed=%s latest=%s", installed_version, release.version);

        s_state = FW_SCREEN_ERROR;
        goto cleanup;
    }

    /*
     * Copy the results before changing the state,
     * so the UI sees the updated information.
     */

    snprintf(s_installed_version, sizeof(s_installed_version), "%s", installed_version);

    snprintf(s_latest_version, sizeof(s_latest_version), "%s", release.version);

    snprintf(s_download_url, sizeof(s_download_url), "%s", release.bin_url);

    if (result > 0) {
        ESP_LOGI(TAG, "New firmware available: %s", s_latest_version);

        s_state = FW_SCREEN_AVAILABLE;
    }
    else if (result == 0) {
        ESP_LOGI(TAG, "Firmware is up to date");
        s_state = FW_SCREEN_UP_TO_DATE;
    }
    else {
        ESP_LOGW(TAG, "Installed firmware is newer than GitHub release");
        s_state = FW_SCREEN_NO_UPDATE;
    }

cleanup:
    s_check_task_handle = NULL;
    vTaskDelete(NULL);
}

/* =========================================================
 * Start release-check task
 * ========================================================= */

static void start_firmware_check_task(void){
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

        ESP_LOGE(TAG, "Failed to create firmware check task");
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
        ESP_LOGI(TAG, "OTA successful.");
        s_state = FW_SCREEN_OTA_DONE;

        TickType_t start_tick = xTaskGetTickCount();

        while (1) {
            TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
            uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(elapsed_ticks);

            if (elapsed_ms >= ESP_RESTART_TIMEOUT_MS) {
                break;
            }

            s_restart_countdown = (ESP_RESTART_TIMEOUT_MS - elapsed_ms + 999) / 1000;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        s_restart_countdown = 0;

        ESP_LOGI(TAG, "Restarting device...");
        esp_restart();

        vTaskDelete(NULL);
        return;
    }

    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));

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

        if (s_state == FW_SCREEN_IDLE || s_state == FW_SCREEN_ERROR || s_state == FW_SCREEN_UP_TO_DATE) {
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

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(2, 10, "FIRMWARE UPDATE");
    display_draw_hline(0, 12, 128);

    switch (s_state) {

    case FW_SCREEN_IDLE:
        snprintf(buf, sizeof(buf), "Installed: %s", s_installed_version);
        display_draw_text(2, 26, buf);
        display_draw_text(2, 44, "Press: Check update");

        break;

    case FW_SCREEN_CHECKING:
        display_draw_text(2, 30, "Checking...");

        break;

    case FW_SCREEN_UP_TO_DATE:
        display_draw_text(2, 30, "Firmware up to date");
        display_draw_text(2, 48, "Press: Check again");

        break;

    case FW_SCREEN_AVAILABLE:

        display_draw_text(2, 22, "Update Available");

        snprintf(buf, sizeof(buf), "Old: %s", s_installed_version);

        display_draw_text(2, 35, buf);

        snprintf(buf, sizeof(buf), "New: %s", s_latest_version);

        display_draw_text(2, 48, buf);
        display_set_color(1);
        display_draw_box(0, 54, 55, 10);
        display_set_color(0);
        display_draw_text(2, 62, "Continue");
        display_set_color(1);

        break;

    case FW_SCREEN_CONFIRM:
        display_draw_text(2, 22, "Install update?");
        snprintf(buf, sizeof(buf), "Version: %s", s_latest_version);
        display_draw_text(2, 34, buf);
        if(s_select_yes){
            display_draw_box(2, 54, 25, 10);
            display_set_color(0);
            display_draw_text(5, 62, "Yes");
            display_set_color(1);
            display_draw_text(85, 62, "No");

        }
        else{
            display_draw_box(80, 54, 25, 10);
            display_set_color(0);
            display_draw_text(85, 62, "No");
            display_set_color(1);
            display_draw_text(2, 62, "Yes");
        }

        break;

    case FW_SCREEN_INSTALLING:
        display_draw_text(2, 30, "Installing...");
        display_draw_text(2, 48, "Please wait");

        break;

    case FW_SCREEN_NO_UPDATE:
        display_draw_text(2, 22, "No newer release");
        display_draw_text(2, 30, "Installed is newer");
        display_draw_text(2, 50, "Press to return");

        break;

    case FW_SCREEN_ERROR:
        display_draw_text(2, 22, "Update failed");
        display_draw_text(2, 30, "Check Wi-Fi/API");
        display_draw_text(2, 50, "Press: Retry");

        break;

    case FW_SCREEN_OTA_DONE:
        char countdown_text[32];
        display_draw_text(2, 25, "Update successful");
        snprintf(countdown_text, sizeof(countdown_text), "Restarting in %ds", s_restart_countdown);
        display_draw_text(2, 40, countdown_text);

        break;

    default:
        break;
    }

    display_update();
}
