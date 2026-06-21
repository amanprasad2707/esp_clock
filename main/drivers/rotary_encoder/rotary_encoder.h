#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


typedef enum {
    ENC_EVT_CW,           /* clockwise tick                    */
    ENC_EVT_CCW,          /* counter-clockwise tick            */
    ENC_EVT_SHORT_PRESS,  /* click released < LONG_MS          */
    ENC_EVT_LONG_PRESS,   /* held >= LONG_MS                   */
} encoder_event_t;

/**
 * @brief Initialise encoder GPIOs and start the ISR/task.
 *        Events are posted to the returned queue.
 *        Queue must be read by the caller (ui_manager).
 */
QueueHandle_t rotary_encoder_init(void);