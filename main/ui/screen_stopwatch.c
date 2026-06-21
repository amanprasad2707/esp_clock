#include "screen_stopwatch.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "u8g2.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

#define MAX_LAPS  20

typedef enum { SW_IDLE = 0, SW_RUN, SW_PAUSED } sw_state_t;

static sw_state_t s_state;
static int64_t    s_start_us;       /* esp_timer when last started */
static int64_t    s_elapsed_us;     /* accumulated elapsed before last pause */

static int64_t    s_laps[MAX_LAPS];
static int        s_n_laps;
static int        s_lap_scroll;     /* top visible lap index */

static bool s_dirty;

#define LAP_VISIBLE 3

/* ---- helpers ---- */
static int64_t current_elapsed(void){
    if (s_state == SW_RUN)
        return s_elapsed_us + (esp_timer_get_time() - s_start_us);
    return s_elapsed_us;
}

static void us_to_parts(int64_t us, int *m, int *s, int *cs /* centiseconds */){
    int64_t total_cs = us / 10000LL;
    *cs = (int)(total_cs % 100);
    int total_s = (int)(total_cs / 100);
    *m = total_s / 60;
    *s = total_s % 60;
}

/* ---- enter ---- */
void screen_stopwatch_enter(void){
    /* Keep previous state if re-entering while running — just redraw */
    s_dirty = true;
}

/* ---- event ---- */
void screen_stopwatch_event(encoder_event_t evt){
    switch (evt) {
        case ENC_EVT_SHORT_PRESS:
            if (s_state == SW_IDLE || s_state == SW_PAUSED) {
                /* Start / resume */
                s_start_us = esp_timer_get_time();
                s_state    = SW_RUN;
            } else {
                /* Record lap */
                if (s_n_laps < MAX_LAPS) {
                    s_laps[s_n_laps++] = current_elapsed();
                    /* Auto-scroll lap list to show newest */
                    if (s_n_laps > LAP_VISIBLE)
                        s_lap_scroll = s_n_laps - LAP_VISIBLE;
                }
            }
            break;

        case ENC_EVT_LONG_PRESS:
            if (s_state == SW_RUN) {
                /* Pause */
                s_elapsed_us = current_elapsed();
                s_state      = SW_PAUSED;
            } else if (s_state == SW_PAUSED) {
                /* Reset */
                s_state      = SW_IDLE;
                s_elapsed_us = 0;
                s_n_laps     = 0;
                s_lap_scroll = 0;
            } else {
                /* Idle: go back to menu */
                ui_manager_goto(SCREEN_MENU);
                return;
            }
            break;

        case ENC_EVT_CW:
            /* Scroll lap list down */
            if (s_lap_scroll < s_n_laps - LAP_VISIBLE)
                s_lap_scroll++;
            break;

        case ENC_EVT_CCW:
            /* Scroll lap list up */
            if (s_lap_scroll > 0)
                s_lap_scroll--;
            break;
    }
    s_dirty = true;
}

/* ---- tick ---- */
void screen_stopwatch_tick(void){
    if (s_state == SW_RUN) s_dirty = true;   /* live update */
    if (!s_dirty) return;
    s_dirty = false;

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(2, 10, "STOPWATCH");
    display_draw_hline(0, 12, 128);

    /* Main elapsed time */
    int64_t elapsed = current_elapsed();
    int em, es, ecs;
    us_to_parts(elapsed, &em, &es, &ecs);

    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d.%02d", em, es, ecs);
    display_set_font(u8g2_font_logisoso26_tf);
    display_draw_text(4, 42, buf);

    /* State hint */
    display_set_font(u8g2_font_6x10_tf);
    if (s_state == SW_IDLE) {
        display_draw_text(2, 55, "Press=start");
    } else if (s_state == SW_RUN) {
        display_draw_text(2, 55, "Press=lap  Hold=pause");
    } else {
        display_draw_text(2, 55, "Press=resume  Hold=reset");
    }

    /* Lap list (compact, below main time if space) */
    if (s_n_laps > 0) {
        display_draw_hline(0, 56, 128);
        display_set_font(u8g2_font_5x8_mf);
        for (int i = 0; i < LAP_VISIBLE && (s_lap_scroll + i) < s_n_laps; i++) {
            int idx = s_lap_scroll + i;
            int lm, ls, lcs;
            us_to_parts(s_laps[idx], &lm, &ls, &lcs);
            char lbuf[64];
            snprintf(lbuf, sizeof(lbuf), "L%02d %02d:%02d.%02d", idx + 1, lm, ls, lcs);
            display_draw_text(2, 58 + i * 8 - 2, lbuf);
        }
    }

    display_update();
}
