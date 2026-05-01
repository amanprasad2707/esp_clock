idf_component_register(
    SRCS
        "main.c"
        "ui/menu.c"
        "rotary_encoder/rotary_encoder.c"
        "display/display.c"
    INCLUDE_DIRS
        "."
        "ui"
        "rotary_encoder"
        "display"
    REQUIRES
        u8g2 u8g2_esp32_hal driver esp_driver_spi esp_driver_i2c
    )
