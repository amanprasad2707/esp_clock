#include "screen_set_date.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "rtc_ds3231.h"
#include "u8g2.h"
#include <stdio.h>

extern ds3231_handle_t ds3231_handle;
extern rtc_date_t g_date;

typedef enum{
    DATE_UI_DATE,
    DATE_UI_MONTH,
    DATE_UI_YEAR,
    DATE_UI_WEEKDAY,
    DATE_UI_BACK,
    DATE_UI_SAVE,
}date_ui_focus_t;

static date_ui_focus_t s_focus;
static int  s_date, s_month, s_year, s_weekday;
static bool s_dirty;

static const uint8_t k_days_in_month[13] = {0, 31,28,31,30,31,30,31,31,30,31,30,31};

static const char *k_weekday_abbr[] = {"","Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static const char *k_month_abbr[] = {"", "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static bool is_leap(int y){
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int max_day(void){
    int d = k_days_in_month[s_month];
    if(s_month == 2 && is_leap(s_year)){
        d = 29;
    }
    return d;
}

void screen_set_date_enter(void){
    ds3231_get_date(&ds3231_handle, &g_date);
    s_date = (g_date.date  >= 1 && g_date.date  <= 31) ? g_date.date  : 1;
    s_month = (g_date.month >= 1 && g_date.month <= 12) ? g_date.month : 1;
    s_year = (g_date.year  >= 2000) ? g_date.year  : 2025;
    s_weekday = (g_date.day >= SUNDAY && g_date.day <= SATURDAY) ? g_date.day : SUNDAY;
    s_focus = DATE_UI_DATE;
    s_dirty = true;
}

static void inc_df(void){
    switch(s_focus){
        case DATE_UI_DATE:
            s_date = (s_date % max_day()) + 1;
            break;

        case DATE_UI_MONTH:
            s_month = (s_month % 12) + 1;   // prevent invalid dates after month change
            if (s_date > max_day()){
                s_date = max_day();
            }
            break;

        case DATE_UI_YEAR:
            s_year++;
            if (s_date > max_day()){    // prevent invalid dates after year change
                s_date = max_day();
            }
            break;

        case DATE_UI_WEEKDAY:
        s_weekday = (s_weekday % 7) + 1;
        break;

        default:
            break;
    }
}

static void dec_df(void){
    switch(s_focus){
        case DATE_UI_DATE:
            s_date = s_date <= 1 ? max_day() : s_date - 1;
            break;

        case DATE_UI_MONTH:
            s_month = s_month <= 1 ? 12 : s_month - 1;
            if (s_date > max_day()){
                s_date = max_day();
            }
            break;

        case DATE_UI_YEAR:
            s_year = s_year > 2000 ? s_year - 1 : s_year;
            if(s_date > max_day()){
                s_date = max_day();
            }
            break;

        case DATE_UI_WEEKDAY:
        s_weekday = s_weekday <= 1 ? 7 : s_weekday - 1;
        break;

        default:
            break;
    }
}

static void save_and_exit(void){
    g_date.date  = s_date;
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
    switch(evt){
        case ENC_EVT_CW:
            if (s_focus == DATE_UI_BACK){
                /* rotate CW on BACK -> go to SAVE */
                s_focus = DATE_UI_SAVE;
            }
            else if(s_focus == DATE_UI_SAVE){
                /* rotate CW on SAVE -> go back to DAY to re-edit */
                s_focus = DATE_UI_DATE;
            }
            else{
                inc_df();
            }
            break;

        case ENC_EVT_CCW:
            if(s_focus == DATE_UI_SAVE){
                /* rotate CCW on SAVE -> go to BACK */
                s_focus = DATE_UI_BACK;
            }
            else if(s_focus == DATE_UI_BACK){
                /* rotate CCW on BACK -> go back to DAY to re-edit */
                s_focus = DATE_UI_DATE;
            }
            else{
                dec_df();
            }
            break;

        case ENC_EVT_SHORT_PRESS:
            if(s_focus == DATE_UI_SAVE){
                save_and_exit();
                return;
            }
            else if(s_focus == DATE_UI_BACK){
                discard_and_exit();
                return;
            }
            else {
                s_focus = (date_ui_focus_t)(s_focus + 1);
            }
            break;

        case ENC_EVT_LONG_PRESS:
            discard_and_exit();
            return;
    }
    s_dirty = true;
}


void screen_set_date_tick(void){
    if(!s_dirty){
        return;
    }
    s_dirty = false;

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(40, 10, "SET DATE");
    display_draw_hline(0, 12, 128);
    

    char preview[20];
    snprintf(preview, sizeof(preview), "%02d %s %04d", s_date, k_month_abbr[s_month], s_year);
    display_set_font(u8g2_font_9x15B_tf);
    display_draw_text(10, 30, preview);

    if(s_focus == DATE_UI_DATE){
        display_draw_hline(9, 32, 18);
    }
    if(s_focus == DATE_UI_MONTH){
        display_draw_hline(37, 32, 27);
    }
    if(s_focus == DATE_UI_YEAR){
        display_draw_hline(72, 32, 36);
    }

    display_set_font(u8g2_font_8x13B_mf);
    display_draw_text(45, 46, k_weekday_abbr[s_weekday]);
    if(s_focus == DATE_UI_WEEKDAY){
        display_draw_hline(47, 48, 21);
    }

    /* BACK button */
    if(s_focus == DATE_UI_BACK){
        display_set_font(u8g2_font_6x10_tf);
        display_draw_box(2, 52, 31, 12);
        display_set_color(0);
        display_draw_text(6, 62, "BACK");
        display_set_color(1);
    }
    else{
        display_set_font(u8g2_font_6x10_tf);
        display_draw_frame(display_get_display_width() - 126, 52, 31, 12);
        display_draw_text(6, 62, "BACK");
    }

    /* SAVE button */
    if(s_focus == DATE_UI_SAVE){
        display_draw_box(88, 52, 31, 12);
        display_set_color(0);
        display_draw_text(92, 62, "SAVE");
        display_set_color(1);
    }
    else{
        display_draw_frame(display_get_display_width() - 40, 52, 31, 12);
        display_draw_text(92, 62, "SAVE");
    }

    display_update();
}