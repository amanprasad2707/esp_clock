#include "rotary_encoder.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdint.h>
#include "config.h"


static QueueHandle_t s_evt_queue;

/* Rotary encoder ISR */
static volatile uint32_t s_last_isr_time = 0;

static void IRAM_ATTR enc_isr(void *arg){
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    int clk = gpio_get_level(ROTARY_ENCODER_CLK);
    int dt  = gpio_get_level(ROTARY_ENCODER_DT);

    if (now - s_last_isr_time < 5) {
        return;   /* debounce: ignore bounces faster than 5ms */
    }

    if(clk == 0){   /* falling edge = one detent */
        s_last_isr_time = now;
        encoder_event_t evt = (dt != clk) ? ENC_EVT_CW : ENC_EVT_CCW;
        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(s_evt_queue, &evt, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/* Button task (handles debounce + long press) */
static void btn_task(void *arg){
    static int64_t press_time = 0;
    static bool pressed = false;

    while(1){
        int level = gpio_get_level(ROTARY_ENCODER_SW);   /* active-low */

        if (level == 0 && !pressed) {
            pressed    = true;
            press_time = esp_timer_get_time();
        }
        else if (level == 1 && pressed) {
            pressed = false;
            int64_t held_ms = (esp_timer_get_time() - press_time) / 1000;
            encoder_event_t evt = (held_ms >= ROTARY_ENCODER_LONG_MS) ? ENC_EVT_LONG_PRESS : ENC_EVT_SHORT_PRESS;
            xQueueSend(s_evt_queue, &evt, pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

QueueHandle_t rotary_encoder_init(void){
    s_evt_queue = xQueueCreate(16, sizeof(encoder_event_t));

    /* CLK / DT — input with pull-up */
    gpio_config_t clk_io = {
        .pin_bit_mask = (1ULL << ROTARY_ENCODER_CLK),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&clk_io);

    gpio_config_t dt_io = {
        .pin_bit_mask = (1ULL << ROTARY_ENCODER_DT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&dt_io);

    /* SW — input with pull-up, polled */
    gpio_config_t sw = {
        .pin_bit_mask = (1ULL << ROTARY_ENCODER_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&sw);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(ROTARY_ENCODER_CLK, enc_isr, NULL);

    xTaskCreate(btn_task, "btn", 2048, NULL, 5, NULL);

    return s_evt_queue;
}
