#include "screen_set_time.h"
#include "ui_manager.h"
#include "display.h"
#include "rtc_ds3231.h"
#include "u8g2.h"
#include <stdio.h>
#include <string.h>

extern ds3231_handle_t ds3231_handle;
extern rtc_time_t   g_time;

/* Fields: 0=hours  1=minutes  2=format(12/24)  3=meridiem(AM/PM) */
typedef enum { FIELD_HOURS = 0, FIELD_MINUTES, FIELD_FORMAT, FIELD_MERIDIEM, FIELD_SAVE, N_FIELDS } field_t;

static field_t s_field;
static int     s_hours;
static int     s_minutes;
static int     s_is_12h;   /* 0=24h, 1=12h */
static int     s_is_pm;    /* 0=AM,  1=PM  (only used in 12h mode) */
static bool    s_dirty;

void screen_set_time_enter(void)
{
    ds3231_get_time(&ds3231_handle, &g_time);
    s_hours   = g_time.hours;
    s_minutes = g_time.minutes;
    s_is_12h  = (g_time.hour_format == HOUR_FORMAT_12) ? 1 : 0;
    s_is_pm   = (g_time.meridiem == PM) ? 1 : 0;
    s_field   = FIELD_HOURS;
    s_dirty   = true;
}

/* ---------- Helpers ---------- */
static void inc_field(void)
{
    switch (s_field) {
        case FIELD_HOURS:
            if (s_is_12h) { s_hours = (s_hours % 12) + 1; }
            else           { s_hours = (s_hours + 1) % 24; }
            break;
        case FIELD_MINUTES:
            s_minutes = (s_minutes + 1) % 60;
            break;
        case FIELD_FORMAT:
            s_is_12h ^= 1;
            /* Convert hour when toggling */
            if (s_is_12h && s_hours > 12) { s_hours -= 12; s_is_pm = 1; }
            if (!s_is_12h && s_is_pm)     { s_hours += 12; }
            break;
        case FIELD_MERIDIEM:
            s_is_pm ^= 1;
            break;
        default: break;
    }
}

static void dec_field(void)
{
    switch (s_field) {
        case FIELD_HOURS:
            if (s_is_12h) { s_hours = s_hours <= 1 ? 12 : s_hours - 1; }
            else           { s_hours = s_hours == 0 ? 23 : s_hours - 1; }
            break;
        case FIELD_MINUTES:
            s_minutes = s_minutes == 0 ? 59 : s_minutes - 1;
            break;
        case FIELD_FORMAT:
            inc_field(); /* only two states, same as inc */
            break;
        case FIELD_MERIDIEM:
            s_is_pm ^= 1;
            break;
        default: break;
    }
}

static void save_and_exit(void)
{
    g_time.hours       = s_hours;
    g_time.minutes     = s_minutes;
    g_time.seconds     = 0;
    g_time.hour_format = s_is_12h ? HOUR_FORMAT_12 : HOUR_FORMAT_24;
    g_time.meridiem    = s_is_pm  ? PM : AM;
    ds3231_set_time(&ds3231_handle, &g_time);
    ui_manager_goto(SCREEN_CLOCK);
}

/* ---------- Event ---------- */
void screen_set_time_event(encoder_event_t evt)
{
    switch (evt) {
        case ENC_EVT_CW:
            if (s_field == FIELD_SAVE) { save_and_exit(); return; }
            inc_field();
            break;
        case ENC_EVT_CCW:
            if (s_field == FIELD_SAVE) break;
            dec_field();
            break;
        case ENC_EVT_SHORT_PRESS:
            if (s_field == FIELD_SAVE) { save_and_exit(); return; }
            s_field = (field_t)(s_field + 1);
            /* Skip MERIDIEM when in 24h mode */
            if (s_field == FIELD_MERIDIEM && !s_is_12h)
                s_field = FIELD_SAVE;
            break;
        case ENC_EVT_LONG_PRESS:
            ui_manager_goto(SCREEN_MENU);
            return;
    }
    s_dirty = true;
}

/* ---------- Render ---------- */
void screen_set_time_tick(void)
{
    if (!s_dirty) return;
    s_dirty = false;

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(2, 10, "SET TIME");
    display_draw_hline(0, 12, 128);

    /* Big time preview */
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_hours, s_minutes);
    display_set_font(u8g2_font_logisoso26_tf);
    display_draw_text(20, 45, buf);

    /* Underline active field */
    u8g2_t *u = display_get_handle();
    /* H underline at ~x=20, M underline at ~x=62 (approx char widths) */
    if (s_field == FIELD_HOURS)   u8g2_DrawHLine(u, 20, 47, 32);
    if (s_field == FIELD_MINUTES) u8g2_DrawHLine(u, 62, 47, 32);

    /* Format + meridiem row */
    display_set_font(u8g2_font_6x12_tf);
    char fmt_str[8], mer_str[4];
    snprintf(fmt_str, sizeof(fmt_str), s_is_12h ? "[12h]" : "[24h]");
    snprintf(mer_str, sizeof(mer_str), s_is_pm  ? "PM"    : "AM");

    /* Highlight active row with inverted box */
    if (s_field == FIELD_FORMAT) {
        u8g2_SetDrawColor(u, 1); u8g2_DrawBox(u, 0, 52, 40, 12); u8g2_SetDrawColor(u, 0);
        display_draw_text(2, 62, fmt_str);
        u8g2_SetDrawColor(u, 1);
    } else {
        display_draw_text(2, 62, fmt_str);
    }

    if (s_is_12h) {
        if (s_field == FIELD_MERIDIEM) {
            u8g2_SetDrawColor(u, 1); u8g2_DrawBox(u, 44, 52, 20, 12); u8g2_SetDrawColor(u, 0);
            display_draw_text(46, 62, mer_str);
            u8g2_SetDrawColor(u, 1);
        } else {
            display_draw_text(46, 62, mer_str);
        }
    }

    /* SAVE button */
    if (s_field == FIELD_SAVE) {
        u8g2_SetDrawColor(u, 1); u8g2_DrawBox(u, 88, 52, 38, 12); u8g2_SetDrawColor(u, 0);
        display_draw_text(92, 62, "SAVE");
        u8g2_SetDrawColor(u, 1);
    } else {
        display_draw_frame(u8g2_GetDisplayWidth(u) - 40, 52, 38, 12);
        display_draw_text(92, 62, "SAVE");
    }

    display_update();
}
