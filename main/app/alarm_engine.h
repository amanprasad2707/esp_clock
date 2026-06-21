#pragma once
#include <stdbool.h>

/* ---- GPIO config ---- */
#define BUZZER_GPIO   25   /* change to your buzzer pin */
/* --------------------- */

/**
 * @brief Start the alarm engine background task.
 *        Monitors g_alarms[], g_time, and responds to buzz requests
 *        from the timer screen.
 */
void alarm_engine_start(void);

/** Trigger buzzer (from timer screen when countdown ends). */
void alarm_engine_start_buzz(void);

/** Silence buzzer. */
void alarm_engine_stop_buzz(void);
