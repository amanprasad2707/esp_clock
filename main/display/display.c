#include "display.h"
#include "u8g2_esp32_hal.h"
#include "esp_err.h"
#include "i2c_hal.h"


static u8g2_t u8g2;
static i2c_master_dev_handle_t oled_dev;

void display_init(void){
    ESP_ERROR_CHECK(i2c_hal_init());

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C << 1);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
}

void display_clear(void){
    u8g2_ClearBuffer(&u8g2);
}

void display_update(void){
    u8g2_SendBuffer(&u8g2);
}

void display_draw_text(int x, int y, const char *text){
    u8g2_DrawStr(&u8g2, x, y, text);
}

void display_set_font(const uint8_t *font){
    u8g2_SetFont(&u8g2, font);
}