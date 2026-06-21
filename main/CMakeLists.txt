idf_component_register(
    SRCS
        "main.c"
        "drivers/rotary_encoder/rotary_encoder.c"
        "drivers/display/display.c"
        "ui/ui_manager.c"
        "ui/screen_clock.c"
        "ui/screen_menu.c"
        "ui/screen_set_time.c"
        "ui/screen_set_date.c"
        "ui/screen_alarms.c"
        "ui/screen_timer.c"
        "ui/screen_stopwatch.c"
        "app/alarm_engine.c"

    INCLUDE_DIRS
        "."
        "ui"
        "drivers/rotary_encoder"
        "drivers/display"
        "app"

    REQUIRES
        u8g2 driver esp_driver_gpio esp_driver_spi esp_driver_i2c rtc_ds3231 esp_timer i2c_hal
    )
