#include "http_ota.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"


static const char *TAG = "http_ota";

#define FIRMWARE_UPGRADE_URL            "https://github.com/amanprasad2707/esp_clock/releases/latest/download/esp_clock.bin"


void http_ota_task(void *arg){
    ESP_LOGI(TAG, "starting http ota");
    vTaskDelay(pdMS_TO_TICKS(20000));

    esp_http_client_config_t config = {
        .url = FIRMWARE_UPGRADE_URL,
        .timeout_ms = 30000,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 8192,
        .buffer_size_tx = 8192,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful");
        esp_restart();
    }
    else{
        ESP_LOGI(TAG, "OTA task deleted");
        vTaskDelete(NULL);
    }
    
}