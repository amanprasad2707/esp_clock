#include "display.h"
#include "u8g2_esp32_hal.h"

#define PIN_SDA 21
#define PIN_SCL 22

static u8g2_t u8g2;

void display_init(void){
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.sda = PIN_SDA;
    hal.scl = PIN_SCL;
    u8g2_esp32_hal_init(hal);

    // -------- Setup display --------
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    // Address (0x3C << 1 = 0x78)
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