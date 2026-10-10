#include "app_fw_update_check.h"
#include <string.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "config.h"


static const char *TAG = "fw_check";
static char response_data[HTTP_RESPONSE_BUFFER_SIZE];

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
    bool overflow;
} response_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt){
    response_buffer_t *response = evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && response != NULL && evt->data_len > 0) {
        size_t incoming_data_len = (size_t)evt->data_len;

        if (incoming_data_len >= response->capacity || response->len >= response->capacity - incoming_data_len) {
            response->overflow = true;
            return ESP_FAIL;
        }

        memcpy(response->data + response->len, evt->data, incoming_data_len);

        response->len += incoming_data_len;
        response->data[response->len] = '\0';
    }

    return ESP_OK;
}

esp_err_t fw_check_latest_release(fw_release_info_t *release){
    if (release == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(release, 0, sizeof(*release));


    response_buffer_t response = {
        .data = response_data,
        .len = 0,
        .capacity = HTTP_RESPONSE_BUFFER_SIZE,
        .overflow = false
    };

    esp_http_client_config_t http_config = {
        .url = RELEASE_API_URL,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .user_data = &response,
        .buffer_size = 2048
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);


    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    esp_http_client_set_header(client, "X-GitHub-Api-Version", "2022-11-28");

    esp_http_client_set_header(client, "User-Agent", "ESP32-Firmware-Updater");

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    if (response.overflow) {
        ESP_LOGE(TAG, "GitHub response exceeds buffer capacity");
        return ESP_ERR_INVALID_SIZE;
    }

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "GitHub request failed: %s, HTTP=%d",
                 esp_err_to_name(err), status);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(response_data);

    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse GitHub JSON");
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *tag = cJSON_GetObjectItemCaseSensitive(
        root, "tag_name");

    const cJSON *assets = cJSON_GetObjectItemCaseSensitive(
        root, "assets");

    if (!cJSON_IsString(tag) || tag->valuestring == NULL || !cJSON_IsArray(assets)){
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Accept tags such as v1.0.3 or 1.0.3.
    const char *version = tag->valuestring;

    if (version[0] == 'v' || version[0] == 'V') {
        version++;
    }

    int version_len = snprintf(release->version, sizeof(release->version), "%s", version);

    if (version_len < 0 || (size_t)version_len >= sizeof(release->version)) {
        cJSON_Delete(root);
        memset(release, 0, sizeof(*release));
        return ESP_ERR_INVALID_SIZE;
    }

    bool asset_found = false;
    const cJSON *asset = NULL;

    cJSON_ArrayForEach(asset, assets) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(asset, "name");

        const cJSON *url = cJSON_GetObjectItemCaseSensitive(asset, "browser_download_url");

        if (cJSON_IsString(name) && name->valuestring != NULL && cJSON_IsString(url) && url->valuestring != NULL && strcmp(name->valuestring, RELEASE_ASSET_NAME) == 0) {

            int url_len = snprintf(release->bin_url, sizeof(release->bin_url), "%s", url->valuestring);

            if (url_len < 0 || (size_t)url_len >= sizeof(release->bin_url)) {
                cJSON_Delete(root);
                memset(release, 0, sizeof(*release));
                return ESP_ERR_INVALID_SIZE;
            }

            asset_found = true;
            break;
        }
    }

    cJSON_Delete(root);

    if (!asset_found) {
        ESP_LOGE(TAG, "Asset %s not found in latest release", RELEASE_ASSET_NAME);
        memset(release, 0, sizeof(*release));
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Latest firmware version: %s", release->version);

    ESP_LOGI(TAG, "Firmware URL: %s", release->bin_url);

    return ESP_OK;
}
