idf_component_register(
    SRCS
        "main.c"
        "ui/menu.c"
        "rotary_encoder/rotary_encoder.c"
    INCLUDE_DIRS
        "."
        "ui"
        "rotary_encoder"
    REQUIRES
        ssd1306 u8g2 driver esp_driver_spi esp_driver_i2c
    )
