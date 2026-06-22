#include <stdint.h>

#include "snake.h"
#include "display_max7219.h"

#define SNAKE_MAX_LEN (DISPLAY_W * DISPLAY_H)

typedef enum
{
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3
} snake_dir_t;

static uint8_t snake_x[SNAKE_MAX_LEN];
static uint8_t snake_y[SNAKE_MAX_LEN];
static uint16_t snake_len = 0U;
static snake_dir_t snake_dir = DIR_RIGHT;
static snake_dir_t snake_next_dir = DIR_RIGHT;
static uint8_t snake_food_x = 0U;
static uint8_t snake_food_y = 0U;
static uint32_t snake_paused = 0U;
static uint32_t snake_game_over = 0U;
static uint32_t snake_blink_state = 1U;
static uint32_t snake_exit_flag = 0U;
static uint32_t rng_state = 0x12345678U;

/*
 * Function: rng_next
 * ------------------
 * Generate next pseudo-random value.
 *
 * Method:
 * - Uses a simple linear congruential generator.
 *
 * Variables:
 * - rng_state: internal generator state.
 *
 * Returns:
 * - Next pseudo-random 32-bit value.
 */
static uint32_t rng_next(void)
{
    rng_state = (rng_state * 1664525U) + 1013904223U;
    return rng_state;
}

/*
 * Function: snake_is_on_body
 * --------------------------
 * Check whether a coordinate is occupied by snake body.
 *
 * Method:
 * - Compares x,y against all segments up to a given limit.
 *
 * Variables:
 * - x: X coordinate to test.
 * - y: Y coordinate to test.
 * - limit: number of body segments to scan.
 * - i: segment index.
 *
 * Returns:
 * - 1U if coordinate is occupied.
 * - 0U otherwise.
 */
static uint32_t snake_is_on_body(uint8_t x, uint8_t y, uint16_t limit)
{
    uint16_t i;

    for (i = 0U; i < limit; i++)
    {
        if (snake_x[i] == x && snake_y[i] == y)
        {
            return 1U;
        }
    }

    return 0U;
}

/*
 * Function: snake_spawn_food
 * --------------------------
 * Place food on a free cell.
 *
 * Method:
 * - Randomly picks coordinates until a free position is found.
 * - Stops after a bounded number of attempts.
 *
 * Variables:
 * - tries: attempt counter.
 * - x: candidate X coordinate.
 * - y: candidate Y coordinate.
 */
static void snake_spawn_food(void)
{
    uint32_t tries = 0U;
    uint8_t x;
    uint8_t y;

    if (snake_len >= SNAKE_MAX_LEN)
    {
        snake_game_over = 1U;
        return;
    }

    do
    {
        x = (uint8_t)(rng_next() % DISPLAY_W);
        y = (uint8_t)(rng_next() % DISPLAY_H);
        tries++;
    }
    while (snake_is_on_body(x, y, snake_len) != 0U && tries < 1000U);

    snake_food_x = x;
    snake_food_y = y;
}

/*
 * Function: snake_is_opposite
 * ---------------------------
 * Check whether two directions are opposite.
 *
 * Method:
 * - Compares direction pairs explicitly.
 *
 * Variables:
 * - a: first direction.
 * - b: second direction.
 *
 * Returns:
 * - 1U if directions are opposite.
 * - 0U otherwise.
 */
static uint32_t snake_is_opposite(snake_dir_t a, snake_dir_t b)
{
    if ((a == DIR_UP && b == DIR_DOWN) ||
        (a == DIR_DOWN && b == DIR_UP) ||
        (a == DIR_LEFT && b == DIR_RIGHT) ||
        (a == DIR_RIGHT && b == DIR_LEFT))
    {
        return 1U;
    }

    return 0U;
}

/*
 * Function: snake_advance
 * -----------------------
 * Advance snake by one simulation step.
 *
 * Method:
 * - Applies pending direction.
 * - Computes next head position.
 * - Detects wall collision and self-collision.
 * - Grows when food is eaten.
 * - Shifts body segments.
 *
 * Variables:
 * - nx: next head X.
 * - ny: next head Y.
 * - growing: whether current move eats food.
 * - limit: body scan limit for self-collision.
 * - i: segment shift index.
 */
static void snake_advance(void)
{
    uint8_t nx;
    uint8_t ny;
    uint32_t growing;
    uint16_t limit;
    uint16_t i;

    snake_dir = snake_next_dir;
    nx = snake_x[0];
    ny = snake_y[0];

    switch (snake_dir)
    {
        case DIR_UP:
            if (ny == 0U)
            {
                snake_game_over = 1U;
                return;
            }
            ny--;
            break;

        case DIR_DOWN:
            if (ny + 1U >= DISPLAY_H)
            {
                snake_game_over = 1U;
                return;
            }
            ny++;
            break;

        case DIR_LEFT:
            if (nx == 0U)
            {
                snake_game_over = 1U;
                return;
            }
            nx--;
            break;

        case DIR_RIGHT:
        default:
            if (nx + 1U >= DISPLAY_W)
            {
                snake_game_over = 1U;
                return;
            }
            nx++;
            break;
    }

    growing = ((nx == snake_food_x) && (ny == snake_food_y)) ? 1U : 0U;

    limit = snake_len;
    if (growing == 0U && limit > 0U)
    {
        limit--;
    }

    if (snake_is_on_body(nx, ny, limit) != 0U)
    {
        snake_game_over = 1U;
        return;
    }

    if (growing != 0U && snake_len < SNAKE_MAX_LEN)
    {
        snake_len++;
    }

    for (i = snake_len - 1U; i > 0U; i--)
    {
        snake_x[i] = snake_x[i - 1U];
        snake_y[i] = snake_y[i - 1U];
    }

    snake_x[0] = nx;
    snake_y[0] = ny;

    if (growing != 0U)
    {
        snake_spawn_food();
    }
}

/*
 * Function: snake_init
 * --------------------
 * Initialize snake subsystem.
 *
 * Method:
 * - Resets the game to default state.
 *
 * Variables:
 * - none.
 */
void snake_init(void)
{
    snake_reset();
}

/*
 * Function: snake_reset
 * ---------------------
 * Reset snake game state.
 *
 * Method:
 * - Places a 4-segment snake near the left-middle area.
 * - Resets directions, flags, blink state and RNG mix.
 * - Spawns new food.
 *
 * Variables:
 * - start_x: initial head X coordinate.
 * - start_y: initial head Y coordinate.
 */
void snake_reset(void)
{
    uint8_t start_x = 10U;
    uint8_t start_y = 3U;

    snake_len = 4U;

    snake_x[0] = start_x;
    snake_y[0] = start_y;

    snake_x[1] = (uint8_t)(start_x - 1U);
    snake_y[1] = start_y;

    snake_x[2] = (uint8_t)(start_x - 2U);
    snake_y[2] = start_y;

    snake_x[3] = (uint8_t)(start_x - 3U);
    snake_y[3] = start_y;

    snake_dir = DIR_RIGHT;
    snake_next_dir = DIR_RIGHT;
    snake_paused = 0U;
    snake_game_over = 0U;
    snake_exit_flag = 0U;
    snake_blink_state = 1U;

    rng_state ^= 0xA5A5A5A5U;
    snake_spawn_food();
}

/*
 * Function: snake_step
 * --------------------
 * Advance snake by one scheduled step.
 *
 * Method:
 * - If game is over, toggles blink state and returns.
 * - If paused, returns without changing state.
 * - Otherwise advances snake by one movement step.
 *
 * Variables:
 * - none.
 */
void snake_step(void)
{
    if (snake_game_over != 0U)
    {
        snake_blink_state = (snake_blink_state == 0U) ? 1U : 0U;
        return;
    }

    if (snake_paused != 0U)
    {
        return;
    }

    snake_advance();
}

/*
 * Function: snake_render
 * ----------------------
 * Render snake and food into MAX7219 framebuffer.
 *
 * Method:
 * - Clears framebuffer.
 * - If game is over and blink state is off, leaves display blank.
 * - Draws food and all snake segments.
 *
 * Variables:
 * - i: segment index.
 */
void snake_render(void)
{
    uint16_t i;

    frame_clear();

    if (snake_game_over != 0U && snake_blink_state == 0U)
    {
        return;
    }

    set_pixel_xy(snake_food_x, snake_food_y);

    for (i = 0U; i < snake_len; i++)
    {
        set_pixel_xy(snake_x[i], snake_y[i]);
    }
}

/*
 * Function: snake_handle_key
 * --------------------------
 * Handle one control key for snake.
 *
 * Method:
 * - P toggles pause.
 * - R resets game immediately.
 * - Q requests exit only while paused.
 * - W/A/S/D change pending direction if not opposite.
 *
 * Variables:
 * - ch: received control key.
 */
void snake_handle_key(char ch)
{
    if (ch == 'p' || ch == 'P')
    {
        snake_paused = (snake_paused == 0U) ? 1U : 0U;
        return;
    }

    if (ch == 'r' || ch == 'R')
    {
        snake_reset();
        return;
    }

    if ((ch == 'q' || ch == 'Q') && (snake_paused != 0U))
    {
        snake_exit_flag = 1U;
        return;
    }

    if (snake_game_over != 0U)
    {
        return;
    }

    if (ch == 'w' || ch == 'W')
    {
        if (snake_is_opposite(snake_dir, DIR_UP) == 0U)
        {
            snake_next_dir = DIR_UP;
        }
        return;
    }

    if (ch == 's' || ch == 'S')
    {
        if (snake_is_opposite(snake_dir, DIR_DOWN) == 0U)
        {
            snake_next_dir = DIR_DOWN;
        }
        return;
    }

    if (ch == 'a' || ch == 'A')
    {
        if (snake_is_opposite(snake_dir, DIR_LEFT) == 0U)
        {
            snake_next_dir = DIR_LEFT;
        }
        return;
    }

    if (ch == 'd' || ch == 'D')
    {
        if (snake_is_opposite(snake_dir, DIR_RIGHT) == 0U)
        {
            snake_next_dir = DIR_RIGHT;
        }
        return;
    }
}

/*
 * Function: snake_is_paused
 * -------------------------
 * Report snake pause state.
 *
 * Method:
 * - Returns internal pause flag.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - 0U when running.
 * - non-zero when paused.
 */
uint32_t snake_is_paused(void)
{
    return snake_paused;
}

/*
 * Function: snake_is_game_over
 * ----------------------------
 * Report snake game-over state.
 *
 * Method:
 * - Returns internal game-over flag.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - 0U when game is still active.
 * - non-zero when game is over.
 */
uint32_t snake_is_game_over(void)
{
    return snake_game_over;
}

/*
 * Function: snake_exit_requested
 * ------------------------------
 * Report whether snake requested exit to running line.
 *
 * Method:
 * - Returns internal exit flag.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - 0U when no exit requested.
 * - non-zero when exit requested.
 */
uint32_t snake_exit_requested(void)
{
    return snake_exit_flag;
}

/*
 * Function: snake_clear_exit_requested
 * ------------------------------------
 * Clear pending snake exit request.
 *
 * Method:
 * - Resets exit flag to 0.
 *
 * Variables:
 * - none.
 */
void snake_clear_exit_requested(void)
{
    snake_exit_flag = 0U;
}