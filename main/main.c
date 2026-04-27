#include "esp_idf_version.h"
#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "menu.h"
#include "rotary_encoder.h"
#include "u8g2.h"


#define OLED_SDA_PIN  21
#define OLED_SCL_PIN  22


typedef enum {
    UI_STATE_CLOCK,
    UI_STATE_MENU
} ui_screen_t;


SSD1306_t dev;
static ui_screen_t current_screen = UI_STATE_CLOCK;


static void ui_task(void *arg){
    /* Queue that receives input events from the rotary encoder */
    QueueHandle_t event_queue = rotary_encoder_get_queue();

    menu_init();
    clock_render();     // start with clock screen

    while (1) {
        ui_event_t event;

        /* Wait indefinitely for next UI event (blocking call) */
        if (xQueueReceive(event_queue, &event, portMAX_DELAY)) {

            switch(current_screen){
                /* clock screen */
                case UI_STATE_CLOCK:
                    /* any interaction, switch to menu */
                    if (event == EVENT_ROTATE_CW || event == EVENT_ROTATE_CCW || event == EVENT_BUTTON_PRESS) {
                        current_screen = UI_STATE_MENU;

                        menu_render();   // draw menu immediately
                    }
                    break;

                /* menu screen */
                case UI_STATE_MENU:
                    /* Update menu state based on input (rotate/button pressed) */
                    menu_handle_event(event);

                    /* Render menu only if state changed */
                    menu_render();
                    break;
            }

            
        }
    }
}

void app_main(void){

    // Initialize display (I2C mode)
    i2c_master_init(&dev, OLED_SDA_PIN, OLED_SCL_PIN, -1);

    // Initialize SSD1306
    ssd1306_init(&dev, 128, 64);

    // Clear screen
    ssd1306_clear_screen(&dev, false);

    rotary_encoder_init();

    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);

}