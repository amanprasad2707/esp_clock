#include "screen_set_date.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "rtc_ds3231.h"
#include "u8g2.h"
#include <stdio.h>

extern ds3231_handle_t ds3231_handle;
extern rtc_date_t   g_date;

typedef enum{
    DF_DAY = 0,
    DF_MONTH,
    DF_YEAR,
    DF_WEEKDAY,
    DF_BACK,
    DF_SAVE,
}date_field_t;

static date_field_t s_field;
static int  s_day, s_month, s_year, s_weekday;
static bool s_dirty;

static const uint8_t k_days_in_month[13] = {0, 31,28,31,30,31,30,31,31,30,31,30,31};

static const char *k_weekday_abbr[] = {"","Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static const char *k_month_abbr[] = {"", "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static bool is_leap(int y) { return (y%4==0 && y%100!=0) || (y%400==0); }
static int  max_day(void)  {
    int d = k_days_in_month[s_month];
    if (s_month == 2 && is_leap(s_year)) d = 29;
    return d;
}

void screen_set_date_enter(void){
    ds3231_get_date(&ds3231_handle, &g_date);
    s_day = (g_date.date  >= 1 && g_date.date  <= 31) ? g_date.date  : 1;
    s_month = (g_date.month >= 1 && g_date.month <= 12) ? g_date.month : 1;
    s_year = (g_date.year  >= 2000) ? g_date.year  : 2025;
    s_weekday = (g_date.day >= SUNDAY && g_date.day <= SATURDAY) ? g_date.day : SUNDAY;
    s_field = DF_DAY;
    s_dirty = true;
}

static void inc_df(void){
    switch (s_field) {
        case DF_DAY:
            s_day = (s_day % max_day()) + 1;
            break;
        case DF_MONTH:
            s_month = (s_month % 12) + 1;
            if (s_day > max_day()) s_day = max_day();
            break;
        case DF_YEAR:
            s_year++;
            if (s_day > max_day()) s_day = max_day();
            break;
        case DF_WEEKDAY:
        s_weekday = (s_weekday % 7) + 1;
        break;
        default:
            break;
    }
}

static void dec_df(void){
    switch (s_field) {
        case DF_DAY:
            s_day = s_day <= 1 ? max_day() : s_day - 1;
            break;
        case DF_MONTH:
            s_month = s_month <= 1 ? 12 : s_month - 1;
            if (s_day > max_day()) s_day = max_day();
            break;
        case DF_YEAR:
            s_year = s_year > 2000 ? s_year - 1 : s_year;
            if (s_day > max_day()) s_day = max_day();
            break;
        case DF_WEEKDAY:
        s_weekday = s_weekday <= 1 ? 7 : s_weekday - 1;
        break;
        default:
            break;
    }
}

static void save_and_exit(void){
    g_date.date  = s_day;
    g_date.month = s_month;
    g_date.year  = s_year;
    g_date.day = s_weekday;
    ds3231_set_date(&ds3231_handle, &g_date);
    ui_manager_goto(SCREEN_CLOCK);
}

static void discard_and_exit(void){
    ui_manager_goto(SCREEN_MENU);
}

void screen_set_date_event(encoder_event_t evt){
    switch (evt) {

        case ENC_EVT_CW:
            if (s_field == DF_BACK){
                /* rotate CW on BACK -> go to SAVE */
                s_field = DF_SAVE;
            } else if (s_field == DF_SAVE){
                /* rotate CW on SAVE -> go back to DAY to re-edit */
                s_field = DF_DAY;
            } else {
                inc_df();
            }
            break;

        case ENC_EVT_CCW:
            if (s_field == DF_SAVE){
                /* rotate CCW on SAVE -> go to BACK */
                s_field = DF_BACK;
            } else if (s_field == DF_BACK){
                /* rotate CCW on BACK -> go back to DAY to re-edit */
                s_field = DF_DAY;
            } else {
                dec_df();
            }
            break;

        case ENC_EVT_SHORT_PRESS:
            if (s_field == DF_SAVE){
                save_and_exit();
                return;
            } else if (s_field == DF_BACK){
                discard_and_exit();
                return;
            } else {
                s_field = (date_field_t)(s_field + 1);
            }
            break;

        case ENC_EVT_LONG_PRESS:
            discard_and_exit();
            return;
    }
    s_dirty = true;
}


void screen_set_date_tick(void){
    if (!s_dirty) return;
    s_dirty = false;

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(40, 10, "SET DATE");
    display_draw_hline(0, 12, 128);
    

    char preview[20];
    snprintf(preview, sizeof(preview), "%02d %s %04d", s_day, k_month_abbr[s_month], s_year);
    display_set_font(u8g2_font_9x15B_tf);
    display_draw_text(10, 30, preview);

    u8g2_t *u = display_get_handle();

    if (s_field == DF_DAY)   u8g2_DrawHLine(u, 9, 32, 18);
    if (s_field == DF_MONTH) u8g2_DrawHLine(u, 37, 32, 27);
    if (s_field == DF_YEAR)  u8g2_DrawHLine(u, 72, 32, 36);

    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(45, 46, k_weekday_abbr[s_weekday]);
    if (s_field == DF_WEEKDAY) u8g2_DrawHLine(u, 45, 48, 18);

    /* BACK button */
    if (s_field == DF_BACK) {
        display_set_font(u8g2_font_6x10_tf);
        u8g2_DrawBox(u, 2, 52, 31, 12);
        u8g2_SetDrawColor(u, 0);
        display_draw_text(6, 62, "BACK");
        u8g2_SetDrawColor(u, 1);
    } else {
        display_set_font(u8g2_font_6x10_tf);
        display_draw_frame(u8g2_GetDisplayWidth(u) - 126, 52, 31, 12);
        display_draw_text(6, 62, "BACK");
    }

    /* SAVE button */
    if (s_field == DF_SAVE) {
        u8g2_DrawBox(u, 88, 52, 31, 12);
        u8g2_SetDrawColor(u, 0);
        display_draw_text(92, 62, "SAVE");
        u8g2_SetDrawColor(u, 1);
    } else {
        display_draw_frame(u8g2_GetDisplayWidth(u) - 40, 52, 31, 12);
        display_draw_text(92, 62, "SAVE");
    }

    display_update();
}