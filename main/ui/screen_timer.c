#include "screen_timer.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "app/alarm_engine.h"
#include "u8g2.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

/*
 * States:
 *   TM_SET   – configure H:M:S before starting
 *   TM_RUN   – counting down
 *   TM_PAUSE – paused mid-run
 *   TM_DONE  – reached zero, buzzing
 */
typedef enum { TM_SET = 0, TM_RUN, TM_PAUSE, TM_DONE } tm_state_t;

/* Which field is active during TM_SET */
typedef enum { TF_HOURS = 0, TF_MINUTES, TF_SECONDS, TF_N } tm_field_t;

static tm_state_t s_state;
static tm_field_t s_set_field;

/* Set values (H/M/S chosen by user) */
static int s_set_h, s_set_m, s_set_s;

/* Countdown: total seconds remaining * 1000 (ms) stored as int64 µs */
static int64_t s_end_us;        /* absolute esp_timer time when timer fires */
static int64_t s_remain_us;     /* used when paused */

static bool s_dirty;

/* ---- helpers ---- */
static int64_t set_total_us(void){
    return (int64_t)(s_set_h * 3600 + s_set_m * 60 + s_set_s) * 1000000LL;
}

static void remain_to_hms(int64_t us, int *h, int *m, int *s){
    int total = (int)(us / 1000000LL);
    if (total < 0) total = 0;
    *h = total / 3600;
    *m = (total % 3600) / 60;
    *s = total % 60;
}

/* ---- enter ---- */
void screen_timer_enter(void){
    s_state     = TM_SET;
    s_set_field = TF_HOURS;
    s_set_h = 0; s_set_m = 5; s_set_s = 0;   /* default 5 minutes */
    s_dirty     = true;
}

/* ---- event ---- */
void screen_timer_event(encoder_event_t evt){
    switch (s_state) {

        /* ---- SET mode: rotate changes field value, press advances field ---- */
        case TM_SET:
            if (evt == ENC_EVT_CW || evt == ENC_EVT_CCW) {
                int dir = (evt == ENC_EVT_CW) ? 1 : -1;
                switch (s_set_field) {
                    case TF_HOURS:
                        s_set_h = (s_set_h + dir + 24) % 24; break;
                    case TF_MINUTES:
                        s_set_m = (s_set_m + dir + 60) % 60; break;
                    case TF_SECONDS:
                        s_set_s = (s_set_s + dir + 60) % 60; break;
                    default: break;
                }
            }
            if (evt == ENC_EVT_SHORT_PRESS) {
                if (s_set_field < TF_SECONDS) {
                    s_set_field = (tm_field_t)(s_set_field + 1);
                } else {
                    /* Start timer */
                    if (set_total_us() > 0) {
                        s_end_us = esp_timer_get_time() + set_total_us();
                        s_state  = TM_RUN;
                    }
                }
            }
            if (evt == ENC_EVT_LONG_PRESS) { ui_manager_goto(SCREEN_MENU); return; }
            break;

        /* ---- RUN: short press pauses, long press resets ---- */
        case TM_RUN:
            if (evt == ENC_EVT_SHORT_PRESS) {
                s_remain_us = s_end_us - esp_timer_get_time();
                s_state     = TM_PAUSE;
            }
            if (evt == ENC_EVT_LONG_PRESS) {
                s_state     = TM_SET;
                s_set_field = TF_HOURS;
            }
            break;

        /* ---- PAUSE: short press resumes, long press resets ---- */
        case TM_PAUSE:
            if (evt == ENC_EVT_SHORT_PRESS) {
                s_end_us = esp_timer_get_time() + s_remain_us;
                s_state  = TM_RUN;
            }
            if (evt == ENC_EVT_LONG_PRESS) {
                s_state     = TM_SET;
                s_set_field = TF_HOURS;
            }
            break;

        /* ---- DONE: any press silences and resets ---- */
        case TM_DONE:
            alarm_engine_stop_buzz();
            s_state     = TM_SET;
            s_set_field = TF_HOURS;
            break;
    }
    s_dirty = true;
}

/* ---- tick (called ~60/s by ui_manager) ---- */
void screen_timer_tick(void){
    /* Check countdown expiry */
    if (s_state == TM_RUN) {
        int64_t remain = s_end_us - esp_timer_get_time();
        if (remain <= 0) {
            s_state = TM_DONE;
            alarm_engine_start_buzz();
            s_dirty = true;
        } else {
            s_dirty = true;   /* update every tick while running */
        }
    }

    if (!s_dirty) return;
    s_dirty = false;

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(2, 10, "TIMER");
    display_draw_hline(0, 12, 128);

    u8g2_t *u = display_get_handle();

    if (s_state == TM_SET) {
        /* Editable H:M:S */
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s_set_h, s_set_m, s_set_s);
        display_set_font(u8g2_font_logisoso26_tf);
        display_draw_text(4, 46, buf);

        /* Underline active field (approx widths for logisoso26: each digit ~16px, colon ~8px) */
        /* H=x4..35, M=x44..75, S=x84..115 */
        int ux = (s_set_field == TF_HOURS) ? 4 : (s_set_field == TF_MINUTES) ? 44 : 84;
        u8g2_DrawHLine(u, ux, 48, 30);

        display_set_font(u8g2_font_6x10_tf);
        if (s_set_field < TF_SECONDS)
            display_draw_text(2, 62, "Press: next field");
        else
            display_draw_text(2, 62, "Press: START");

    } else if (s_state == TM_DONE) {
        display_set_font(u8g2_font_9x15B_tf);
        display_draw_text(20, 40, "TIME'S UP!");
        display_set_font(u8g2_font_6x10_tf);
        display_draw_text(10, 58, "Press any key to stop");

    } else {
        /* RUN or PAUSE */
        int h, m, s;
        int64_t remain = (s_state == TM_RUN) ? (s_end_us - esp_timer_get_time()) : s_remain_us;
        remain_to_hms(remain, &h, &m, &s);

        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        display_set_font(u8g2_font_logisoso26_tf);
        display_draw_text(4, 46, buf);

        display_set_font(u8g2_font_6x10_tf);
        if (s_state == TM_PAUSE) {
            display_draw_text(2, 62, "PAUSED  Press=resume");
        } else {
            display_draw_text(2, 62, "Press=pause  Hold=reset");
        }

        /* Progress bar */
        int total_s = s_set_h * 3600 + s_set_m * 60 + s_set_s;
        int rem_s   = h * 3600 + m * 60 + s;
        if (total_s > 0) {
            int bar_w = (int)((128LL * rem_s) / total_s);
            u8g2_DrawFrame(u, 0, 54, 128, 5);
            u8g2_DrawBox(u,   0, 54, bar_w, 5);
        }
    }

    display_update();
}
