#include "rotary_encoder.h"
#include "driver/gpio.h"
#include "esp_log.h"


#define ENC_CLK     23      // rotary encoder clock
#define ENC_DT      2       // rotary encoder direction
#define ENC_SW      4       // rotary encoder button


static const char *TAG = "rotary_encoder";
static QueueHandle_t encoder_queue;
static volatile int last_clk_level = 0;


// ----------- Rotary ISR -----------
static void IRAM_ATTR encoder_isr(void *arg){
    static uint32_t last_event_tick = 0;        // last time encoder event was accepted

    uint32_t current_tick = xTaskGetTickCountFromISR();     // current RTOS tick count

    // Debounce: ignore events within 2 ticks
    if ((current_tick - last_event_tick) < 2) {
        return;
    }

    last_event_tick = current_tick;

    int clk_level = gpio_get_level(ENC_CLK);    // current state of CLK pin
    int dt_level  = gpio_get_level(ENC_DT);     // current state of DT pin

    ui_event_t rotation_event;

    // Direction detection
    if (dt_level != clk_level) {
        rotation_event = EVENT_ROTATE_CW;
    }
    else {
        rotation_event = EVENT_ROTATE_CCW;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(encoder_queue, &rotation_event, &higher_priority_task_woken);

    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}


// ----------- Button ISR -----------
static void IRAM_ATTR button_isr(void *arg){
    ui_event_t event = EVENT_BUTTON_PRESS;
    xQueueSendFromISR(encoder_queue, &event, NULL);
}


// ----------- Init -----------
void rotary_encoder_init(void){
    encoder_queue = xQueueCreate(10, sizeof(ui_event_t));
    if(encoder_queue == NULL){
        ESP_LOGE(TAG, "Failed to create encoder queue");
        return;
    }

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask =
            (1ULL << ENC_CLK) |
            (1ULL << ENC_DT) |
            (1ULL << ENC_SW),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };

    gpio_config(&io_conf);

    last_clk_level = gpio_get_level(ENC_CLK);

    esp_err_t err =  gpio_install_isr_service(0);
    ESP_ERROR_CHECK(err);


    gpio_isr_handler_add(ENC_CLK, encoder_isr, NULL);
    gpio_isr_handler_add(ENC_SW, button_isr, NULL);
}


// ----------- Queue Getter -----------
QueueHandle_t rotary_encoder_get_queue(void){
    return encoder_queue;
}