#include "screen_stopwatch.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "u8g2.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>


typedef enum{
    SW_IDLE = 0,
    SW_RUN,
    SW_PAUSED,
    SW_RESET
}sw_state_t;

static sw_state_t s_state;
static int64_t s_start_us;       /* esp_timer when last started */
static int64_t s_elapsed_us;     /* accumulated elapsed before last pause */

static bool s_dirty;


/* ---- helpers ---- */
static int64_t current_elapsed(void){
    if (s_state == SW_RUN){
        return s_elapsed_us + (esp_timer_get_time() - s_start_us);
    }

    return s_elapsed_us;
}

static void us_to_parts(int64_t us, int *m, int *s, int *cs){ /* centiseconds */
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
                s_state = SW_RUN;
            }
            
            else if(s_state == SW_RUN){
                /* Pause */
                s_elapsed_us = current_elapsed();
                s_state = SW_PAUSED;
            }

            else if(s_state == SW_RESET){
                /* Reset */
                s_elapsed_us = 0;
                s_state = SW_IDLE;
            }
            break;

        case ENC_EVT_LONG_PRESS:
            ui_manager_goto(SCREEN_MENU);
            break;

        case ENC_EVT_CW:
        case ENC_EVT_CCW:
            if(s_state == SW_PAUSED || s_state == SW_RESET){
                s_state = (s_state == SW_PAUSED) ? SW_RESET : SW_PAUSED;
            }
            break;
    }
    s_dirty = true;
}

/* ---- tick ---- */
void screen_stopwatch_tick(void){
    if (s_state == SW_RUN){
        s_dirty = true;   /* live update */
    }

    if (!s_dirty){
        return;
    }

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
    
    display_set_font(u8g2_font_6x10_tf);
    
    if(s_state == SW_PAUSED){
        display_set_color(1);
        display_draw_box(10, 50, 45, 13);
        display_set_color(0);
        display_draw_text(14, 60, "RESUME");
        display_set_color(1);
        display_draw_text(84, 60, "RESET");
    }

    else if(s_state == SW_RESET){
        display_draw_text(14, 60, "RESUME");
        display_set_color(1);
        display_draw_box(80, 50, 40, 13);
        display_set_color(0);
        display_draw_text(84, 60, "RESET");
        display_set_color(1);
    }

    display_update();
}
