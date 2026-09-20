#include "display.h"
#include "esp_err.h"
#include "i2c_hal.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>



static u8g2_t u8g2;
static i2c_master_dev_handle_t oled_dev = NULL;

static const char *TAG = "display";


static uint8_t tx_buffer[256];
static size_t tx_length = 0;

/* ESP-IDF u8g2 I2C callback */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);





void display_init(void){
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    u8x8_SetI2CAddress(&u8g2.u8x8, OLED_I2C_ADDR << 1);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
}

void display_clear(void){
    u8g2_ClearBuffer(&u8g2);
}

void display_update(void){
    u8g2_SendBuffer(&u8g2);
}

void display_set_color(uint8_t color){
    u8g2_SetDrawColor(&u8g2, color);
}

void display_draw_text(int x, int y, const char *text){
    u8g2_DrawStr(&u8g2, x, y, text);
}

void display_set_font(const uint8_t *font){
    u8g2_SetFont(&u8g2, font);
}

void display_draw_hline(int x, int y, int w){
    u8g2_DrawHLine(&u8g2, x, y, w);
}

void display_draw_box(int x, int y, int w, int h){
    u8g2_DrawBox(&u8g2, x, y, w, h);
}

void display_draw_frame(int x, int y, int w, int h){
    u8g2_DrawFrame(&u8g2, x, y, w, h);
}

void display_bitmap(int x, int y, int w, int h, const uint8_t *bitmap){
    u8g2_DrawXBM(&u8g2, x, y, w, h, bitmap);
}

u8g2_t *display_get_handle(void){
    return &u8g2;
}

uint16_t display_get_display_width(void){
    return u8g2_GetDisplayWidth(&u8g2);
}




uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr){
    switch(msg){
        case U8X8_MSG_BYTE_INIT:{
            if(oled_dev == NULL){
                esp_err_t err = i2c_hal_add_device(OLED_I2C_ADDR, OLED_I2C_FREQ_HZ, &oled_dev);

                if(err != ESP_OK){
                    ESP_LOGE(TAG, "Failed to add OLED device");
                }
            }
            break;
        }

        case U8X8_MSG_BYTE_START_TRANSFER:{
            tx_length = 0;
            break;
        }

        case U8X8_MSG_BYTE_SEND:{
            if((tx_length + arg_int) > sizeof(tx_buffer)){
                ESP_LOGE(TAG, "TX buffer overflow");
                return 0;
            }

            memcpy(&tx_buffer[tx_length], arg_ptr, arg_int);
            tx_length += arg_int;
            break;
        }

        case U8X8_MSG_BYTE_END_TRANSFER:{
            if(oled_dev != NULL && tx_length > 0){
                esp_err_t err = i2c_master_transmit(oled_dev, tx_buffer, tx_length, -1);

                if(err != ESP_OK){
                    ESP_LOGE(TAG,"OLED transmit failed: %s", esp_err_to_name(err));
                }
            }

            break;
        }

        default:
            break;
    }

    return 0;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr){
    switch(msg){
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;

        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;

        default:
            break;
    }

    return 0;
}
