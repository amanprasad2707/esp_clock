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

static const char *TAG = "main";

/* Globals shared across screens (declared extern in screen_*.c) */
ds3231_handle_t ds3231_handle;
rtc_time_t g_time;
rtc_date_t g_date;

void app_main(void){
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

}