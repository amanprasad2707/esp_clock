#pragma once
#include "rotary_encoder.h"

void screen_fw_update_enter(void);
void screen_fw_update_event(encoder_event_t evt);
void screen_fw_update_tick(void);