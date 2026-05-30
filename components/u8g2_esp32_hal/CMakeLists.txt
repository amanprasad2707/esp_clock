idf_component_register(
    SRCS "u8g2_esp32_hal.c"
    INCLUDE_DIRS "."
    REQUIRES driver u8g2 esp_driver_gpio esp_driver_i2c i2c_hal
)