#include "u8g2_esp32_hal.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "i2c_hal.h"

#define OLED_I2C_ADDR      0x3C
#define OLED_I2C_FREQ_HZ   400000

static const char *TAG = "U8G2_HAL";

static i2c_master_dev_handle_t oled_dev = NULL;


static uint8_t tx_buffer[256];
static size_t tx_length = 0;

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