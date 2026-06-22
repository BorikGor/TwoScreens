#include <stdint.h>
#include <string.h>

#include "fire.h"
#include "display_ws2812.h"


#define FIRE_BRIGHTNESS  8U

#define BLUE_ROWS        2U
#define BLUE_MIN_HEAT    40U
#define BLUE_MAX_HEAT    180U
#define BLUE_MAX_VALUE   8U

#define BASE_MIN         40U
#define BASE_MAX         180U
#define SPARK_PROB       25U
#define COOL_BASE        10U
#define COOL_Y           3U

static uint8_t heat[WS2812_PANEL_H][WS2812_PANEL_W];
static uint8_t next_heat[WS2812_PANEL_H][WS2812_PANEL_W];
static uint32_t rng_state = 0x12345678U;

/*
 * Function: rng8
 * --------------
 * Generate one pseudo-random byte.
 *
 * Method:
 * - Uses a linear congruential generator.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - One pseudo-random 8-bit value.
 */
static uint8_t rng8(void)
{
    rng_state = (rng_state * 1664525U) + 1013904223U;
    return (uint8_t)(rng_state >> 24);
}

/*
 * Function: scale8
 * ----------------
 * Apply simple 8-bit brightness scaling.
 *
 * Method:
 * - Computes (value * scale) / 255.
 *
 * Variables:
 * - value: unscaled channel value.
 * - scale: brightness scale 0..255.
 *
 * Returns:
 * - Scaled channel value.
 */
static uint8_t scale8(uint8_t value, uint8_t scale)
{
    return (uint8_t)(((uint16_t)value * (uint16_t)scale) / 255U);
}

/*
 * Function: add_base_blue_tint
 * ----------------------------
 * Add a subtle blue tint near the base of the flame.
 *
 * Method:
 * - Blue is applied only to the lower rows.
 * - Blue appears only in a medium-hot range.
 * - The hottest cells keep their yellow spark-like color.
 *
 * Variables:
 * - y: visual row index where 0 is bottom.
 * - heat_value: current heat value at the cell.
 * - b: pointer to blue channel to modify.
 * - blue_level: computed blue contribution.
 * - span: heat span of the blue region.
 */
static void add_base_blue_tint(
    uint8_t y,
    uint8_t heat_value,
    uint8_t *b
)
{
    uint8_t blue_level;
    uint8_t span;

    if (y >= BLUE_ROWS)
    {
        return;
    }

    if (heat_value < BLUE_MIN_HEAT)
    {
        return;
    }

    if (heat_value > BLUE_MAX_HEAT)
    {
        return;
    }

    span = (uint8_t)(BLUE_MAX_HEAT - BLUE_MIN_HEAT);
    blue_level = (uint8_t)(
        ((uint16_t)(heat_value - BLUE_MIN_HEAT) * BLUE_MAX_VALUE) / span
    );

    if (blue_level > *b)
    {
        *b = blue_level;
    }
}

/*
 * Function: heat_to_rgb
 * ---------------------
 * Convert flame heat value to RGB color.
 *
 * Method:
 * - 0..84    -> black to red
 * - 85..169  -> red to orange/yellow
 * - 170..255 -> bright warm yellow
 *
 * Variables:
 * - heat_value: input flame intensity 0..255.
 * - r, g, b: output channel pointers.
 * - t: local transition value.
 */
static void heat_to_rgb(
    uint8_t heat_value,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b
)
{
    uint8_t t;

    if (heat_value < 85U)
    {
        *r = (uint8_t)(heat_value * 3U);
        *g = 0U;
        *b = 0U;
    }
    else if (heat_value < 170U)
    {
        t = (uint8_t)(heat_value - 85U);
        *r = 255U;
        *g = (uint8_t)(t * 3U);
        *b = 0U;
    }
    else
    {
        t = (uint8_t)(heat_value - 170U);
        *r = 255U;
        *g = (uint8_t)(200U + ((uint16_t)t * 55U) / 85U);
        *b = 0U;
    }
}

/*
 * Function: flame_seed_bottom
 * ---------------------------
 * Generate heat on the bottom row.
 *
 * Method:
 * - Gives each bottom cell a warm random base value.
 * - Occasionally injects stronger spark-like bursts.
 *
 * Variables:
 * - x: column index.
 * - base: generated base heat value.
 */
static void flame_seed_bottom(void)
{
    uint8_t x;
    uint8_t base;

    for (x = 0U; x < WS2812_PANEL_W; x++)
    {
        base = (uint8_t)(BASE_MIN +
            (rng8() % (BASE_MAX - BASE_MIN + 1U)));
        next_heat[0U][x] = base;

        if (rng8() < SPARK_PROB)
        {
            next_heat[0U][x] = 255U;
        }
    }
}

/*
 * Function: flame_step
 * --------------------
 * Advance the flame simulation by one frame.
 *
 * Method:
 * - Seeds the bottom row with random heat.
 * - Propagates heat upward from row y-1 to row y.
 * - Applies lateral blur and stronger cooling at higher rows.
 *
 * Variables:
 * - x: column index.
 * - y: row index.
 * - sum: accumulated neighbor heat.
 * - count: number of contributing neighbors.
 * - value: averaged source heat.
 * - cool: cooling value for this cell.
 */
static void flame_step(void)
{
    uint8_t x;
    uint8_t y;
    uint16_t sum;
    uint8_t count;
    uint8_t value;
    uint8_t cool;

    flame_seed_bottom();

    for (y = 1U; y < WS2812_PANEL_H; y++)
    {
        for (x = 0U; x < WS2812_PANEL_W; x++)
        {
            sum = 0U;
            count = 0U;

            sum += heat[y - 1U][x];
            count++;

            if (x > 0U)
            {
                sum += heat[y - 1U][x - 1U];
                count++;
            }

            if (x + 1U < WS2812_PANEL_W)
            {
                sum += heat[y - 1U][x + 1U];
                count++;
            }

            if (y > 1U)
            {
                sum += heat[y - 2U][x];
                count++;
            }

            value = (uint8_t)(sum / count);
            cool = (uint8_t)(COOL_BASE + (y * COOL_Y) +
                (rng8() & 0x07U));

            if (value > cool)
            {
                next_heat[y][x] = (uint8_t)(value - cool);
            }
            else
            {
                next_heat[y][x] = 0U;
            }
        }
    }

    memcpy(heat, next_heat, sizeof(heat));
}

/*
 * Function: flame_render
 * ----------------------
 * Convert the heat map into WS2812 RGB pixels.
 *
 * Method:
 * - Clears the output buffer.
 * - Converts each heat cell to RGB color.
 * - Writes color to the visually addressed pixel.
 *
 * Variables:
 * - x: column index.
 * - y: row index.
 * - r, g, b: rendered color channels.
 */
static void flame_render(void)
{
    uint8_t x;
    uint8_t y;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    display_ws2812_clear();

    for (y = 0U; y < WS2812_PANEL_H; y++)
    {
        for (x = 0U; x < WS2812_PANEL_W; x++)
        {
            heat_to_rgb(heat[y][x], &r, &g, &b);
            add_base_blue_tint(y, heat[y][x], &b);

            display_ws2812_set_pixel_rgb(
                x,
                y,
                scale8(r, FIRE_BRIGHTNESS),
                scale8(g, FIRE_BRIGHTNESS),
                scale8(b, FIRE_BRIGHTNESS)
            );
        }
    }
}

/*
 * Function: fire_init
 * -------------------
 * Initialize the fire simulation state.
 *
 * Method:
 * - Clears internal heat buffers.
 * - Generates and renders one initial fire frame.
 *
 * Variables:
 * - none.
 */
void fire_init(void)
{
    memset(heat, 0, sizeof(heat));
    memset(next_heat, 0, sizeof(next_heat));

    flame_step();
    flame_render();
    display_ws2812_show();
}

/*
 * Function: fire_step
 * -------------------
 * Advance and render one scheduled fire frame.
 *
 * Method:
 * - Runs one flame simulation step.
 * - Renders the heat map into WS2812 RGB buffer.
 * - Sends the frame to the WS2812 panel.
 *
 * Variables:
 * - none.
 */
void fire_step(void)
{
    flame_step();
    flame_render();
    display_ws2812_show();
}