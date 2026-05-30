idf_component_register(
    SRCS
        "main.c"
        "ui/menu.c"
        "rotary_encoder/rotary_encoder.c"
        "display/display.c"
        i2c_hal/i2c_hal.c

    INCLUDE_DIRS
        "."
        "ui"
        "rotary_encoder"
        "display"
        "i2c_hal"
    REQUIRES
        u8g2 u8g2_esp32_hal driver esp_driver_spi esp_driver_i2c
    )
