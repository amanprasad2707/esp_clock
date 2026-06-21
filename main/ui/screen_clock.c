#include "screen_clock.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "rtc_ds3231.h"
#include "u8g2.h"
#include <stdio.h>


/* Shared RTC handles, defined in main.c */
extern ds3231_handle_t ds3231_handle;
extern rtc_time_t   g_time;
extern rtc_date_t   g_date;

void screen_clock_enter(void) { /* nothing to init */ }

void screen_clock_event(encoder_event_t evt){
    switch (evt) {
        case ENC_EVT_SHORT_PRESS:
        case ENC_EVT_LONG_PRESS:
        case ENC_EVT_CCW:
        case ENC_EVT_CW:
            ui_manager_goto(SCREEN_MENU);
            break;
        default:
            break;
    }
}

void screen_clock_tick(void){
    char time_str[20], sec_str[4], date_str[20], temp_str[8];
    float temp;

    ds3231_get_time(&ds3231_handle, &g_time);

    static uint8_t last_second = 60;
    if (g_time.seconds == last_second) return;
    last_second = g_time.seconds;

    ds3231_get_date(&ds3231_handle, &g_date);
    ds3231_get_temperature(&ds3231_handle, &temp);

    snprintf(time_str, sizeof(time_str), "%02d:%02d", g_time.hours, g_time.minutes);
    snprintf(sec_str,  sizeof(sec_str),  "%02d", g_time.seconds);
    snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d", g_date.date, g_date.month, g_date.year);
    snprintf(temp_str, sizeof(temp_str), "%d%cC", (int)temp, 176);

    display_clear();

    display_set_font(u8g2_font_logisoso26_tf);
    display_draw_text(22, 40, time_str);

    display_set_font(u8g2_font_6x13_tf);
    display_draw_text(10, 25, g_time.meridiem == AM ? "AM" : "PM");
    display_draw_text(102, 45, sec_str);

    display_set_font(u8g2_font_6x12_tf);
    display_draw_text(22, 64,  date_str);
    display_draw_text(105, 64, temp_str);

    display_update();
}
