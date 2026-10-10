#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "rotary_encoder.h"
#include "display.h"
#include "ui_manager.h"
#include "alarm_engine.h"

#include "rtc_ds3231.h"
#include "i2c_hal.h"
#include "config.h"
#include "nvs_flash.h"
#include "app_wifi.h"

static const char *TAG = "main";

/* Globals shared across screens (declared extern in screen_*.c) */
ds3231_handle_t ds3231_handle;
rtc_time_t g_time;
rtc_date_t g_date;

#define HTTP_OTA_TASK_STACK_SIZE      8096
#define HTTP_OTA_TASK_PRIORITY         4


TaskHandle_t httpOtaTaskHandle;
StaticTask_t httpOtaTaskBuffer;
StackType_t httpOtaTaskStack[HTTP_OTA_TASK_STACK_SIZE];


void app_main(void){

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    /* Init I2C hal */
    ESP_ERROR_CHECK(i2c_hal_init());

    /* Init RTC */
    ds3231_init(i2c_hal_get_bus(), RTC_I2C_ADDR, &ds3231_handle);
    ESP_LOGI(TAG, "RTC OK");

    /* Init OLED */
    display_init();
    ESP_LOGI(TAG, "Display OK");

    /* Init Encoder and returns event queue */
    QueueHandle_t enc_q = rotary_encoder_init();
    ESP_LOGI(TAG, "Rotary Encoder OK");

    /* Alarm engine (background task) */
    alarm_engine_start();
    ESP_LOGI(TAG, "Alarm engine OK");

    /* UI manager (owns all screens) */
    ui_manager_start(enc_q);
    ESP_LOGI(TAG, "UI manager started");

    connect_to_wifi();

}