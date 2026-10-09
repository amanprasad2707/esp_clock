#include "screen_menu.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "u8g2.h"
#include <string.h>
#include <stdio.h>

/* ---------- Menu items ---------- */
typedef struct {
    const char *label;
    screen_id_t target;
} menu_item_t;

static const menu_item_t k_items[] = {
    { "Set Time",  SCREEN_SET_TIME  },
    { "Set Date",  SCREEN_SET_DATE  },
    { "Alarms",    SCREEN_ALARMS    },
    { "Timer",     SCREEN_TIMER     },
    { "Stopwatch", SCREEN_STOPWATCH },
    { "FW Update", SCREEN_FW_UPDATE },
};
#define N_ITEMS   (sizeof(k_items) / sizeof(k_items[0]))
#define ROW_H     13   /* pixels per row */
#define VISIBLE   4    /* rows visible at once */

static int s_sel    = 0;
static int s_scroll = 0;   /* index of top visible item */
static bool s_dirty = true;

void screen_menu_enter(void)
{
    s_sel    = 0;
    s_scroll = 0;
    s_dirty  = true;
}

void screen_menu_event(encoder_event_t evt)
{
    switch (evt) {
        case ENC_EVT_CW:
            if (s_sel < (int)N_ITEMS - 1) {
                s_sel++;
                if (s_sel >= s_scroll + VISIBLE) s_scroll++;
                s_dirty = true;
            }
            break;
        case ENC_EVT_CCW:
            if (s_sel > 0) {
                s_sel--;
                if (s_sel < s_scroll) s_scroll--;
                s_dirty = true;
            }
            break;
        case ENC_EVT_SHORT_PRESS:
            ui_manager_goto(k_items[s_sel].target);
            break;
        case ENC_EVT_LONG_PRESS:
            ui_manager_goto(SCREEN_CLOCK);
            break;
    }
}

void screen_menu_tick(void)
{
    if (!s_dirty) return;
    s_dirty = false;

    display_clear();

    /* Title bar */
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(2, 10, "MENU");
    display_draw_hline(0, 12, 128);

    display_set_font(u8g2_font_6x12_tf);

    for (int i = 0; i < VISIBLE && (s_scroll + i) < (int)N_ITEMS; i++) {
        int idx = s_scroll + i;
        int y   = 14 + i * ROW_H;   /* top of row */

        if (idx == s_sel) {
            /* Inverted highlight */
            u8g2_t *u = display_get_handle();
            u8g2_SetDrawColor(u, 1);
            u8g2_DrawBox(u, 0, y, 128, ROW_H);
            u8g2_SetDrawColor(u, 0);   /* white text on black box */
            display_draw_text(3, y + ROW_H - 2, k_items[idx].label);
            u8g2_SetDrawColor(u, 1);   /* restore */
        } else {
            display_draw_text(3, y + ROW_H - 2, k_items[idx].label);
        }
    }

    /* Scroll indicator */
    if ((int)N_ITEMS > VISIBLE) {
        int bar_h   = 50;
        int thumb_h = bar_h / N_ITEMS;
        int thumb_y = 14 + (s_scroll * (bar_h - thumb_h)) / (N_ITEMS - VISIBLE);
        display_draw_frame(124, 14, 4, bar_h);
        display_draw_box(124, thumb_y, 4, thumb_h);
    }

    display_update();
}
