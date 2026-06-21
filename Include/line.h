#ifndef LINE_H
#define LINE_H

#include <stdint.h>

void line_init(void);
void line_tick(uint32_t now_ms);

void line_set_text(const char *msg);
void line_pause(void);
void line_resume(void);
void line_set_speed(uint32_t ms);

uint32_t line_is_paused(void);

#endif