#pragma once
#include "rotary_encoder.h"

typedef enum{
    SW_IDLE = 0,
    SW_RUN,
    SW_PAUSED,
    SW_RESET
}sw_state_t;

void screen_stopwatch_enter(void);
void screen_stopwatch_event(encoder_event_t evt);
void screen_stopwatch_tick(void);
