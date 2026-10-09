#include "ui_manager.h"
#include "rotary_encoder.h"

#include "screen_clock.h"
#include "screen_menu.h"
#include "screen_set_time.h"
#include "screen_set_date.h"
#include "screen_alarms.h"
#include "screen_timer.h"
#include "screen_stopwatch.h"
#include "screen_fw_update.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"



/* Screen descriptor */
typedef struct {
    void (*on_enter)(void);
    void (*on_event)(encoder_event_t);
    void (*on_tick)(void);   /* called every ~16 ms for rendering + timers */
} screen_desc_t;

static const screen_desc_t k_screens[] = {
    [SCREEN_CLOCK] = {screen_clock_enter, screen_clock_event, screen_clock_tick},
    [SCREEN_MENU] = {screen_menu_enter, screen_menu_event, screen_menu_tick},
    [SCREEN_SET_TIME] = {screen_set_time_enter, screen_set_time_event, screen_set_time_tick},
    [SCREEN_SET_DATE] = {screen_set_date_enter, screen_set_date_event, screen_set_date_tick},
    [SCREEN_ALARMS] = {screen_alarms_enter, screen_alarms_event, screen_alarms_tick},
    [SCREEN_TIMER] = {screen_timer_enter, screen_timer_event, screen_timer_tick},
    [SCREEN_STOPWATCH] = {screen_stopwatch_enter, screen_stopwatch_event, screen_stopwatch_tick},
    [SCREEN_FW_UPDATE] = {screen_fw_update_enter, screen_fw_update_event, screen_fw_update_tick},
};
#define N_SCREENS (sizeof(k_screens) / sizeof(k_screens[0]))  // number of screens

static screen_id_t s_current_screen = SCREEN_CLOCK;
static QueueHandle_t s_enc_q;       // rotary encoder queue

void ui_manager_goto(screen_id_t screen){
    if (screen >= N_SCREENS){
        return;
    }

    s_current_screen = screen;

    if (k_screens[screen].on_enter){    // checks if this screen have enter funciton or not ((k_screens[screen].on_enter != NULL)
        k_screens[screen].on_enter();
    }
}

static void ui_task(void *arg){
    ui_manager_goto(SCREEN_CLOCK);

    while(1){
        encoder_event_t evt;
        /* Wait up to 16 ms for an encoder event (controls framerate to ~60Hz) */
        if (xQueueReceive(s_enc_q, &evt, pdMS_TO_TICKS(16)) == pdTRUE) {
            /* Process the event, then drain any other pending events to prevent UI lag */
            screen_id_t initial_screen = s_current_screen;
            do {
                if (k_screens[s_current_screen].on_event){
                    k_screens[s_current_screen].on_event(evt);
                }
                
                /* Stop draining events if a screen transition occurred */
                if (s_current_screen != initial_screen){
                    break;
                }

            } while (xQueueReceive(s_enc_q, &evt, 0) == pdTRUE);
        }

        /* Always run tick for rendering / countdown updates */
        if (k_screens[s_current_screen].on_tick){
            k_screens[s_current_screen].on_tick();
        }
    }
}

void ui_manager_start(QueueHandle_t enc_queue){
    s_enc_q = enc_queue;
    xTaskCreate(ui_task, "ui", 4096, NULL, 4, NULL);
}
