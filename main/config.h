#pragma once

#define OLED_I2C_ADDR               0x3C
#define OLED_I2C_FREQ_HZ            400000

#define RTC_I2C_ADDR                0x68

#define ROTARY_ENCODER_CLK          23
#define ROTARY_ENCODER_DT           2       // rotary encoder direction
#define ROTARY_ENCODER_SW           4       // rotary encoder button
#define ROTARY_ENCODER_LONG_MS      600     // ms held to fire LONG_PRESS


#define FIRMWARE_UPGRADE_URL            "https://github.com/amanprasad2707/esp_clock/releases/latest/download/esp_clock.bin"
#define RELEASE_API_URL                 "https://api.github.com/repos/amanprasad2707/esp_clock/releases/latest"

#define RELEASE_ASSET_NAME "esp_clock.bin"
#define HTTP_RESPONSE_BUFFER_SIZE 12288
