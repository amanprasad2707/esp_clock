#include <string.h>
#include <stdio.h>

#include "menu.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "display.h"
#include "rtc_ds3231.h"
#include "ui.h"

static bool needs_redraw = true;
static const char *TAG = "menu";

rtc_time_t time;
rtc_date_t date;

extern ds3231_handle_t ds3231_handle;
extern ui_screen_t current_screen;

static uint8_t calculate_dow(uint8_t d, uint8_t m, uint16_t y) {
    if (m < 3) { m += 12; y -= 1; }
    int k = y % 100;
    int j = y / 100;
    int h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h == 0) ? 7 : h; // DS3231 mapping: 1=Sun, 2=Mon... 7=Sat
}

// ---------- Menu Structure ----------
typedef struct menu {
    const char *label;
    struct menu *children;
    int child_count;
    void (*action)(void);
} menu_item_t;


// ---------- Menu State --------------
typedef struct {
    menu_item_t *items;     // current menu list
    int item_count;         // number of items in current menu
    int selected_index;     // currently highlighted item
} menu_state_t;

typedef enum {
    MENU_MODE_LIST,
    MENU_MODE_EDIT_DATETIME
} menu_mode_t;

static menu_mode_t current_menu_mode = MENU_MODE_LIST;
static rtc_time_t edit_time;
static rtc_date_t edit_date;

typedef enum {
    RTC_FIELD_HOUR,
    RTC_FIELD_MINUTE,
    RTC_FIELD_SECOND,
    RTC_FIELD_AM_PM,
    RTC_FIELD_DAY,
    RTC_FIELD_MONTH,
    RTC_FIELD_YEAR,
    RTC_FIELD_TIME_FORMAT,
    RTC_FIELD_SAVE,
    RTC_FIELD_EXIT
} rtc_datetime_field_t;

typedef enum {
    DATETIME_MODE_SELECT,
    DATETIME_MODE_EDIT
} datetime_mode_t;

static menu_state_t menu_state;
rtc_datetime_field_t rtc_datetime_field;
static datetime_mode_t datetime_mode = DATETIME_MODE_SELECT;
extern menu_item_t main_menu[];

// ---------- Dummy Actions ----------
void action_clock(void) {
    current_screen = UI_STATE_CLOCK;
    display_clear();
}
void action_timer() {}
void action_stopwatch() {}
void action_settings() {}
void action_wifi() {}
void action_display() {}
void action_back_to_main(void);

void action_set_datetime(void) {
    ds3231_get_time(&ds3231_handle, &edit_time);
    ds3231_get_date(&ds3231_handle, &edit_date);
    current_menu_mode = MENU_MODE_EDIT_DATETIME;
    rtc_datetime_field = RTC_FIELD_HOUR;
    needs_redraw = true;
}

void action_back_to_main(void) {
    menu_state.items = main_menu;
    menu_state.item_count = 4;
    menu_state.selected_index = 0;
    needs_redraw = true;
}

// ---------- Submenu ----------
menu_item_t settings_menu[] = {
    {"DATE & TIME", NULL, 0, action_set_datetime},
    {"WiFi", NULL, 0, action_wifi},
    {"Display", NULL, 0, action_display},
    {"Back", NULL, 0, action_back_to_main},
};


// ---------- Main Menu ----------
menu_item_t main_menu[] = {
    {"Clock", NULL, 0, action_clock},
    {"Timer", NULL, 0, action_timer},
    {"Stopwatch", NULL, 0, action_stopwatch},
    {"Settings", settings_menu, 4, NULL},
};


void menu_init(void){
    menu_state.items = main_menu;
    menu_state.item_count = 4;
    menu_state.selected_index = 0;

    needs_redraw = true;
}


// ---------- Navigation ----------
void menu_next(void){
    menu_state.selected_index = (menu_state.selected_index + 1) % menu_state.item_count;
    needs_redraw = true;
}

void menu_prev(void){
    menu_state.selected_index = 
        (menu_state.selected_index - 1 + menu_state.item_count) % menu_state.item_count;

    needs_redraw = true;
}

void menu_select(void){
    /* Get pointer to currently selected menu item */
    menu_item_t *selected_item = &menu_state.items[menu_state.selected_index];

    /* If item has a submenu, switch to it */
    if(selected_item->children != NULL){
        menu_state.items = selected_item->children;     // load submenu
        menu_state.item_count = selected_item->child_count;
        menu_state.selected_index = 0;
        needs_redraw = true;    // update display
    } 
    /* If no submenu, execute associated action */
    else if(selected_item->action){
        selected_item->action();
    }
}

/* 
TODO(optimization):
 Current implementation redraws all 8 lines every time.
Optimize by:
1. Tracking previous selected_index
2. Redrawing only:
    - previously selected line (remove cursor)
    - currently selected line (add cursor)
3. Perform full redraw only when:
    - menu changes (submenu enter/exit)
    - item_count changes
    - labels/content change
This will reduce I2C traffic and improve responsiveness. */

void menu_render(void){
    if (!needs_redraw){
        return;
    }

    display_clear();   // clear buffer once

    if (current_menu_mode == MENU_MODE_EDIT_DATETIME) {
        display_set_font(u8g2_font_6x10_tf);
        display_draw_text(0, 10, " < Date/Time >");

        char f_hh[7], f_mm[7], f_ss[7], f_ap[7], f_fmt[7], f_dd[7], f_mo[7], f_yy[9];
        
        snprintf(f_hh, sizeof(f_hh), rtc_datetime_field == RTC_FIELD_HOUR ? "[%02d]" : " %02d ", edit_time.hours);
        snprintf(f_mm, sizeof(f_mm), rtc_datetime_field == RTC_FIELD_MINUTE ? "[%02d]" : " %02d ", edit_time.minutes);
        snprintf(f_ss, sizeof(f_ss), rtc_datetime_field == RTC_FIELD_SECOND ? "[%02d]" : " %02d ", edit_time.seconds);
        snprintf(f_ap, sizeof(f_ap), rtc_datetime_field == RTC_FIELD_AM_PM ? "[%s]" : " %s ", edit_time.meridiem == AM ? "AM" : "PM");
        snprintf(f_fmt, sizeof(f_fmt), rtc_datetime_field == RTC_FIELD_TIME_FORMAT ? "[%s]" : " %s ", edit_time.hour_format == HOUR_FORMAT_12 ? "12H" : "24H");
        snprintf(f_dd, sizeof(f_dd), rtc_datetime_field == RTC_FIELD_DAY ? "[%02d]" : " %02d ", edit_date.date);
        snprintf(f_mo, sizeof(f_mo), rtc_datetime_field == RTC_FIELD_MONTH ? "[%02d]" : " %02d ", edit_date.month);
        snprintf(f_yy, sizeof(f_yy), rtc_datetime_field == RTC_FIELD_YEAR ? "[%04d]" : " %04d ", edit_date.year);

        char buf[64];
        if(edit_time.hour_format == HOUR_FORMAT_12) {
            snprintf(buf, sizeof(buf), "%s:%s:%s%s", f_hh, f_mm, f_ss, f_ap);
        }
        else {
            snprintf(buf, sizeof(buf), "   %s:%s:%s", f_hh, f_mm, f_ss);
        }

        display_draw_text(0, 24, buf);

        snprintf(buf, sizeof(buf), "Fmt: %s", f_fmt);
        display_draw_text(0, 36, buf);

        snprintf(buf, sizeof(buf), "%s/%s/%s", f_dd, f_mo, f_yy);
        display_draw_text(0, 48, buf);

        const char* days[] = {"", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        snprintf(buf, sizeof(buf), "Day:  %s ", (edit_date.day >= 1 && edit_date.day <= 7) ? days[edit_date.day] : "");
        display_draw_text(0, 60, buf);

        display_draw_text(75, 60, rtc_datetime_field == RTC_FIELD_SAVE ? "[ SAVE ]" : "  SAVE  ");
        display_draw_text(75, 36, rtc_datetime_field == RTC_FIELD_EXIT ? "[ EXIT ]" : "  EXIT  ");

    } else {
        display_set_font(u8g2_font_6x10_tf);

    for (int i = 0; i < 8; i++) {
        char buffer[20];

        if (i < menu_state.item_count) {
            if (i == menu_state.selected_index)
                snprintf(buffer, sizeof(buffer), "> %s", menu_state.items[i].label);
            else
                snprintf(buffer, sizeof(buffer), "  %s", menu_state.items[i].label);
        } else {
            snprintf(buffer, sizeof(buffer), " ");
        }

        // Y position = line height * index
        int y = (i + 1) * 10;   // 10px font height

        display_draw_text(0, y, buffer);
    }
    }

    display_update();   // send buffer ONCE

    needs_redraw = false;
}


void menu_handle_event(ui_event_t event)
{
    if(current_menu_mode == MENU_MODE_EDIT_DATETIME)
    {
        /* ---------------- SELECT MODE ---------------- */
        if(datetime_mode == DATETIME_MODE_SELECT)
        {
            switch(event)
            {
                case EVENT_ROTATE_CW:
                    rtc_datetime_field++;

                    if(rtc_datetime_field > RTC_FIELD_EXIT)
                        rtc_datetime_field = RTC_FIELD_HOUR;

                    break;

                case EVENT_ROTATE_CCW:
                    if(rtc_datetime_field == RTC_FIELD_HOUR)
                        rtc_datetime_field = RTC_FIELD_EXIT;
                    else
                        rtc_datetime_field--;

                    break;

                case EVENT_BUTTON_PRESS:

                    if(rtc_datetime_field == RTC_FIELD_SAVE)
                    {
                        ds3231_set_time(&ds3231_handle, &edit_time);
                        ds3231_set_date(&ds3231_handle, &edit_date);

                        current_menu_mode = MENU_MODE_LIST;
                    }
                    else if(rtc_datetime_field == RTC_FIELD_EXIT)
                    {
                        current_menu_mode = MENU_MODE_LIST;
                    }
                    else
                    {
                        datetime_mode = DATETIME_MODE_EDIT;
                    }

                    break;

                default:
                    break;
            }
        }

        /* ---------------- EDIT MODE ---------------- */
        else
        {
            if(event == EVENT_BUTTON_PRESS)
            {
                datetime_mode = DATETIME_MODE_SELECT;
            }
            else if(event == EVENT_ROTATE_CW ||
                    event == EVENT_ROTATE_CCW)
            {
                int dir = (event == EVENT_ROTATE_CW) ? 1 : -1;

                switch(rtc_datetime_field)
                {
                    case RTC_FIELD_HOUR:

                        if(edit_time.hour_format == HOUR_FORMAT_12)
                        {
                            if(dir > 0)
                                edit_time.hours =
                                    (edit_time.hours % 12) + 1;
                            else
                                edit_time.hours =
                                    (edit_time.hours == 1) ?
                                    12 :
                                    edit_time.hours - 1;
                        }
                        else
                        {
                            if(dir > 0)
                                edit_time.hours =
                                    (edit_time.hours + 1) % 24;
                            else
                                edit_time.hours =
                                    (edit_time.hours == 0) ?
                                    23 :
                                    edit_time.hours - 1;
                        }

                        break;

                    case RTC_FIELD_MINUTE:

                        edit_time.minutes =
                            (dir > 0) ?
                            (edit_time.minutes + 1) % 60 :
                            (edit_time.minutes == 0 ?
                                59 :
                                edit_time.minutes - 1);

                        break;

                    case RTC_FIELD_SECOND:

                        edit_time.seconds =
                            (dir > 0) ?
                            (edit_time.seconds + 1) % 60 :
                            (edit_time.seconds == 0 ?
                                59 :
                                edit_time.seconds - 1);

                        break;

                    case RTC_FIELD_AM_PM:

                        edit_time.meridiem =
                            (edit_time.meridiem == AM) ?
                            PM :
                            AM;

                        break;

                    case RTC_FIELD_DAY:

                        edit_date.date =
                            (dir > 0) ?
                            ((edit_date.date % 31) + 1) :
                            ((edit_date.date == 1) ?
                                31 :
                                edit_date.date - 1);

                        break;

                    case RTC_FIELD_MONTH:

                        edit_date.month =
                            (dir > 0) ?
                            ((edit_date.month % 12) + 1) :
                            ((edit_date.month == 1) ?
                                12 :
                                edit_date.month - 1);

                        break;

                    case RTC_FIELD_YEAR:

                        if(dir > 0)
                        {
                            if(edit_date.year < 2199)
                                edit_date.year++;
                        }
                        else
                        {
                            if(edit_date.year > 2000)
                                edit_date.year--;
                        }

                        break;

                    case RTC_FIELD_TIME_FORMAT:

                        if(edit_time.hour_format == HOUR_FORMAT_12)
                        {
                            edit_time.hour_format = HOUR_FORMAT_24;
                        }
                        else
                        {
                            edit_time.hour_format = HOUR_FORMAT_12;
                        }

                        break;

                    default:
                        break;
                }

                edit_date.day =
                    calculate_dow(
                        edit_date.date,
                        edit_date.month,
                        edit_date.year);
            }
        }

        needs_redraw = true;
        return;
    }

    /* Normal menu navigation */

    switch(event)
    {
        case EVENT_ROTATE_CW:
            menu_next();
            break;

        case EVENT_ROTATE_CCW:
            menu_prev();
            break;

        case EVENT_BUTTON_PRESS:
            menu_select();
            break;

        default:
            break;
    }
}


void clock_render(void){
    // TODO: replace with RTC data
    char time_str[20];
    char date_str[20];
    char sec_str[4];
    float temp;
    char temp_str[8];

    ds3231_get_time(&ds3231_handle, &time);

    /* Only redraw when the seconds change to avoid screen flicker and excessive I2C traffic */
    static uint8_t last_second = 60;
    if (time.seconds == last_second) {
        return;
    }
    last_second = time.seconds;

    ds3231_get_date(&ds3231_handle, &date);
    ds3231_get_temperature(&ds3231_handle, &temp);

    ESP_LOGI(TAG, "%d:%d:%d %s %s", time.hours, time.minutes, time.seconds, time.hour_format == HOUR_FORMAT_12 ? "12" : "24", time.meridiem == AM ? "AM" : "PM");

    snprintf(time_str, sizeof(time_str), "%02d:%02d", time.hours, time.minutes);
    snprintf(sec_str, sizeof(sec_str), "%02d", time.seconds);
    snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d", date.date, date.month, date.year);
    snprintf(temp_str, sizeof(temp_str), "%d%cC", (int)temp, 176);

    display_clear();

    // -------- BIG TIME --------
    display_set_font(u8g2_font_logisoso26_tf);

    // center horizontally (approx)
    display_draw_text(22, 40, time_str);
    display_set_font(u8g2_font_6x13_tf);
    display_draw_text(10, 25, time.meridiem == AM ? "AM" : "PM");
    display_draw_text(102, 45, sec_str);

    // -------- DATE --------
    display_set_font(u8g2_font_6x12_tf);
    display_draw_text(22, 64, date_str);
    display_draw_text(105, 64, temp_str);

    display_update();
}
