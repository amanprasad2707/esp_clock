#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_events.h"


void rotary_encoder_init(void);
QueueHandle_t rotary_encoder_get_queue(void);



