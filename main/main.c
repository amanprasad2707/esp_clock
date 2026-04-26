#include "esp_idf_version.h"
#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "menu.h"
#include "rotary_encoder.h"


#define OLED_SDA_PIN  21
#define OLED_SCL_PIN  22

SSD1306_t dev;


static void ui_task(void *arg){
    /* Queue that receives input events from the rotary encoder */
    QueueHandle_t event_queue = rotary_encoder_get_queue();

    menu_init();
    menu_render();   // display initial screen

    while (1) {
        ui_event_t event;

        /* Wait indefinitely for next UI event (blocking call) */
        if (xQueueReceive(event_queue, &event, portMAX_DELAY)) {

            /* Update menu state based on input (rotate/button pressed) */
            menu_handle_event(event);

            /* Render menu only if state changed */
            menu_render();
        }
    }
}

void app_main(void){

    // Initialize display (I2C mode)
    i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, -1);

    // Initialize SSD1306
    ssd1306_init(&dev, 128, 64);

    // Clear screen
    ssd1306_clear_screen(&dev, false);

    rotary_encoder_init();

    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);

}