#include "screen_alarms.h"
#include "ui_manager.h"
#include "drivers/display/display.h"
#include "u8g2.h"
#include <stdio.h>
#include <string.h>

/* Persistent alarm config (read by alarm_engine too) */
alarm_cfg_t g_alarms[N_ALARMS] = {
    { .hours = 7,  .minutes = 0, .enabled = false },
    { .hours = 8,  .minutes = 0, .enabled = false },
};

/*
 * State machine inside this screen:
 *   AL_LIST   – select alarm slot
 *   AL_EDIT_H – edit hours
 *   AL_EDIT_M – edit minutes
 *   AL_EDIT_EN– edit enabled
 */
typedef enum { AL_LIST = 0, AL_EDIT_H, AL_EDIT_M, AL_EDIT_EN } al_state_t;

static al_state_t s_state;
static int        s_slot;    /* 0 or 1 */
static bool       s_dirty;

void screen_alarms_enter(void){
    s_state = AL_LIST;
    s_slot  = 0;
    s_dirty = true;
}

void screen_alarms_event(encoder_event_t evt){
    alarm_cfg_t *a = &g_alarms[s_slot];

    switch (s_state) {
        /* ---- List: choose slot ---- */
        case AL_LIST:
            if (evt == ENC_EVT_CW  && s_slot < N_ALARMS - 1) { s_slot++; }
            if (evt == ENC_EVT_CCW && s_slot > 0)             { s_slot--; }
            if (evt == ENC_EVT_SHORT_PRESS)  { s_state = AL_EDIT_H; }
            if (evt == ENC_EVT_LONG_PRESS)   { ui_manager_goto(SCREEN_MENU); return; }
            break;

        /* ---- Edit hours ---- */
        case AL_EDIT_H:
            if (evt == ENC_EVT_CW)          { a->hours = (a->hours + 1) % 24; }
            if (evt == ENC_EVT_CCW)         { a->hours = a->hours == 0 ? 23 : a->hours - 1; }
            if (evt == ENC_EVT_SHORT_PRESS)  { s_state = AL_EDIT_M; }
            if (evt == ENC_EVT_LONG_PRESS)   { s_state = AL_LIST; }
            break;

        /* ---- Edit minutes ---- */
        case AL_EDIT_M:
            if (evt == ENC_EVT_CW)          { a->minutes = (a->minutes + 1) % 60; }
            if (evt == ENC_EVT_CCW)         { a->minutes = a->minutes == 0 ? 59 : a->minutes - 1; }
            if (evt == ENC_EVT_SHORT_PRESS)  { s_state = AL_EDIT_EN; }
            if (evt == ENC_EVT_LONG_PRESS)   { s_state = AL_LIST; }
            break;

        /* ---- Toggle enabled ---- */
        case AL_EDIT_EN:
            if (evt == ENC_EVT_CW || evt == ENC_EVT_CCW) { a->enabled ^= true; }
            if (evt == ENC_EVT_SHORT_PRESS) { s_state = AL_LIST; }   /* done */
            if (evt == ENC_EVT_LONG_PRESS)  { s_state = AL_LIST; }
            break;
    }
    s_dirty = true;
}

static void draw_alarm_row(int slot, int y, bool selected){
    alarm_cfg_t *a = &g_alarms[slot];
    char buf[64];
    snprintf(buf, sizeof(buf), "A%d  %02d:%02d  %s", slot + 1, a->hours, a->minutes, a->enabled ? "ON " : "OFF");

    u8g2_t *u = display_get_handle();
    if (selected) {
        u8g2_SetDrawColor(u, 1);
        u8g2_DrawBox(u, 0, y - 11, 128, 13);
        u8g2_SetDrawColor(u, 0);
        display_draw_text(2, y, buf);
        u8g2_SetDrawColor(u, 1);
    } else {
        display_draw_text(2, y, buf);
    }
}

void screen_alarms_tick(void){
    if (!s_dirty) return;
    s_dirty = false;

    display_clear();
    display_set_font(u8g2_font_6x10_tf);
    display_draw_text(2, 10, "ALARMS");
    display_draw_hline(0, 12, 128);

    display_set_font(u8g2_font_6x12_tf);

    for (int i = 0; i < N_ALARMS; i++) {
        draw_alarm_row(i, 26 + i * 15, (s_state == AL_LIST && i == s_slot));
    }

    /* Edit sub-screen overlay */
    if (s_state != AL_LIST) {
        alarm_cfg_t *a = &g_alarms[s_slot];

        /* Big time preview at bottom */
        char big[8];
        snprintf(big, sizeof(big), "%02d:%02d", a->hours, a->minutes);
        display_set_font(u8g2_font_9x15B_tf);

        /* underline active field */
        u8g2_t *u = display_get_handle();
        display_draw_text(40, 50, big);
        if (s_state == AL_EDIT_H) u8g2_DrawHLine(u, 40, 51, 27);
        if (s_state == AL_EDIT_M) u8g2_DrawHLine(u, 76, 51, 27);
    }

    display_update();
}
