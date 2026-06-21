#pragma once

#include "u8g2.h"

void display_init(void);
void display_clear(void);
void display_update(void);
void display_draw_text(int x, int y, const char *text);
void display_set_font(const uint8_t *font);
void display_draw_hline(int x, int y, int w);
void display_draw_box(int x, int y, int w, int h);
void display_draw_frame(int x, int y, int w, int h);

/* Expose raw u8g2 handle for advanced callers */
u8g2_t *display_get_handle(void);