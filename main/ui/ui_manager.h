#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    SCREEN_CLOCK = 0,
    SCREEN_MENU,
    SCREEN_SET_TIME,
    SCREEN_SET_DATE,
    SCREEN_ALARMS,
    SCREEN_TIMER,
    SCREEN_STOPWATCH,
} screen_id_t;

/**
 * @brief Start the UI manager task.
 *        @param enc_queue  Queue returned by encoder_init().
 */
void ui_manager_start(QueueHandle_t enc_queue);

/** Navigate to a specific screen (call from any screen handler). */
void ui_manager_goto(screen_id_t screen);
