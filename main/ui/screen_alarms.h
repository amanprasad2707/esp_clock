#pragma once
#include "rotary_encoder.h"
#include <stdbool.h>
#include <stdint.h>

#define N_ALARMS 2

typedef struct{
    uint8_t hours;
    uint8_t minutes;
    bool enabled;
} alarm_cfg_t;

/* Shared alarm config — read by alarm_engine */
extern alarm_cfg_t g_alarms[N_ALARMS];

void screen_alarms_enter(void);
void screen_alarms_event(encoder_event_t evt);
void screen_alarms_tick(void);
