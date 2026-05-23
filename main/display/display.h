#pragma once

#include "u8g2.h"

void display_init(void);
void display_clear(void);
void display_update(void);
void display_draw_text(int x, int y, const char *text);
void display_set_font(const uint8_t *font);