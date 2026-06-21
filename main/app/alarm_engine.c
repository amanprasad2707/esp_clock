#include "alarm_engine.h"
#include "ui/screen_alarms.h"
#include "rtc_ds3231.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "alarm_engine";

extern ds3231_handle_t ds3231_handle;
extern rtc_time_t   g_time;

/* ---- Buzz state ---- */
static volatile bool s_buzzing   = false;
static volatile bool s_buzz_req  = false;   /* set by timer/stopwatch */

void alarm_engine_start_buzz(void){
    s_buzz_req = true;
}

void alarm_engine_stop_buzz(void){
    s_buzzing = false; s_buzz_req = false;
    gpio_set_level(BUZZER_GPIO, 0);
}

/* ---- Simple on/off beep pattern ---- */
static void beep_pattern(void){
    /* 3 short beeps */
    for (int i = 0; i < 3 && s_buzzing; i++) {
        gpio_set_level(BUZZER_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(150));
        gpio_set_level(BUZZER_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ---- Task ---- */
static void alarm_task(void *arg){
    static uint8_t last_minute = 0xFF;
    static bool    alarm_fired[N_ALARMS] = { false };

    for (;;) {
        /* Check buzz requests */
        if (s_buzz_req) {
            s_buzz_req = false;
            s_buzzing  = true;
        }

        if (s_buzzing) {
            beep_pattern();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;   /* keep buzzing until stopped */
        }

        /* Check alarms once per new minute */
        ds3231_get_time(&ds3231_handle, &g_time);
        if (g_time.minutes != last_minute) {
            last_minute = g_time.minutes;

            for (int i = 0; i < N_ALARMS; i++) {
                alarm_cfg_t *a = &g_alarms[i];
                if (a->enabled
                    && a->hours   == g_time.hours
                    && a->minutes == g_time.minutes
                    && !alarm_fired[i])
                {
                    ESP_LOGI(TAG, "Alarm %d fired!", i + 1);
                    alarm_fired[i] = true;
                    s_buzzing      = true;
                } else if (a->minutes != g_time.minutes) {
                    alarm_fired[i] = false;   /* reset for next day */
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void alarm_engine_start(void){
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(BUZZER_GPIO, 0);

    xTaskCreate(alarm_task, "alarm_engine", 3072, NULL, 3, NULL);
}
