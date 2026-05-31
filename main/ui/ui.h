#pragma once

typedef enum {
    UI_STATE_CLOCK,
    UI_STATE_MENU,
    UI_STATE_DATETIME_EDITOR
} ui_screen_t;

extern ui_screen_t current_screen;