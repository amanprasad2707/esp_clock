#include <string.h>


#include "menu.h"
#include "esp_idf_version.h"
#include "ssd1306.h"
#include "esp_log.h"

extern SSD1306_t dev;       // defined in main
static bool needs_redraw = true;
static const char *TAG = "menu";

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

    for (int i = 0; i < 8; i++) {
        char buffer[20];

        if (i < menu_state.item_count) {
            if (i == menu_state.selected_index)
                snprintf(buffer, sizeof(buffer), "> %-14s", menu_state.items[i].label);
            else
                snprintf(buffer, sizeof(buffer), "  %-14s", menu_state.items[i].label);

        } else {
            snprintf(buffer, sizeof(buffer), "                ");
        }

        ssd1306_display_text(&dev, i, buffer, 16, false);
    }

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