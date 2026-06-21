#pragma once

#define OLED_I2C_ADDR               0x3C
#define OLED_I2C_FREQ_HZ            400000

#define RTC_I2C_ADDR                0x68

#define ROTARY_ENCODER_CLK          23
#define ROTARY_ENCODER_DT           2       // rotary encoder direction
#define ROTARY_ENCODER_SW           4       // rotary encoder button
#define ROTARY_ENCODER_LONG_MS      600     // ms held to fire LONG_PRESS