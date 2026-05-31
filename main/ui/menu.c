#include <string.h>
#include <stdio.h>

#include "menu.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "display.h"
#include "rtc_ds3231.h"

static bool needs_redraw = true;
static const char *TAG = "menu";

rtc_time_t time;
rtc_date_t date;

extern ds3231_handle_t ds3231_handle;


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


// ---------- Dummy Actions ----------
void action_clock() {}
void action_timer() {}
void action_stopwatch() {}
void action_settings() {}
void action_set_time() {}
void action_wifi() {}
void action_display() {}


// ---------- Submenu ----------
menu_item_t settings_menu[] = {
    {"Set Time", NULL, 0, action_set_time},
    {"WiFi", NULL, 0, action_wifi},
    {"Display", NULL, 0, action_display},
};


// ---------- Main Menu ----------
menu_item_t main_menu[] = {
    {"Clock", NULL, 0, action_clock},
    {"Timer", NULL, 0, action_timer},
    {"Stopwatch", NULL, 0, action_stopwatch},
    {"Settings", settings_menu, 3, NULL},
};


static menu_state_t menu_state;


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

    display_update();   // send buffer ONCE

    needs_redraw = false;
}


void menu_handle_event(ui_event_t event){
    switch(event){
        case EVENT_ROTATE_CW:{
            menu_next();
            ESP_LOGI(TAG, "CW");
            break;
        }

        case EVENT_ROTATE_CCW:{
            menu_prev();
            ESP_LOGI(TAG, "CCW");
            break;
        }

        case EVENT_BUTTON_PRESS:{
            menu_select();
            ESP_LOGI(TAG, "button pressed");
            break;
        }

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
    snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d", date.day, date.month, date.year);
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