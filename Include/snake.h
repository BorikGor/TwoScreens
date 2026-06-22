#ifndef SNAKE_H
#define SNAKE_H

void snake_init(void);
void snake_reset(void);
void snake_step(void);
void snake_render(void);
void snake_handle_key(char ch);

uint32_t snake_is_paused(void);
uint32_t snake_is_game_over(void);

uint32_t snake_exit_requested(void);
void snake_clear_exit_requested(void);

#endif