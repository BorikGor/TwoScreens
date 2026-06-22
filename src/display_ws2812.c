#include <stdint.h>
#include <string.h>

#include "stm32f1xx.h"
#include "display_ws2812.h"

/*
 * WS2812 data output pin.
 *
 * Method:
 * - PB8 used as single-wire data output.
 *
 * Variables:
 * - WS_PORT: GPIO port for WS2812 data.
 * - WS_SET_MASK: BSRR mask to drive line high.
 * - WS_CLR_MASK: BSRR mask to drive line low.
 */
#define WS_PORT                 GPIOB
#define WS_SET_MASK             GPIO_BSRR_BS8
#define WS_CLR_MASK             GPIO_BSRR_BR8

/*
 * WS2812 raw pixel buffer.
 *
 * Method:
 * - Stored in GRB order, 3 bytes per LED.
 *
 * Variables:
 * - ws_pixels: output pixel buffer.
 */
static uint8_t ws_pixels[WS2812_LED_COUNT * 3U];

/*
 * Function: delay_cycles
 * ----------------------
 * Small busy-loop delay.
 *
 * Method:
 * - Repeatedly executes NOP instructions.
 *
 * Variables:
 * - cycles: loop counter.
 */
static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--)
    {
        __asm volatile ("nop");
    }
}

/*
 * Function: ws_high
 * -----------------
 * Drive the WS2812 data line high.
 *
 * Method:
 * - Uses GPIO BSRR for atomic set operation.
 *
 * Variables:
 * - none.
 */
static inline __attribute__((always_inline)) void ws_high(void)
{
    WS_PORT->BSRR = (uint32_t)WS_SET_MASK;
}

/*
 * Function: ws_low
 * ----------------
 * Drive the WS2812 data line low.
 *
 * Method:
 * - Uses GPIO BSRR reset half for atomic clear operation.
 *
 * Variables:
 * - none.
 */
static inline __attribute__((always_inline)) void ws_low(void)
{
    WS_PORT->BSRR = (uint32_t)WS_CLR_MASK;
}

/*
 * Function: ws_send_bit_1
 * -----------------------
 * Send logic '1' to WS2812.
 *
 * Method:
 * - Keeps line high longer, then low for the rest of the bit cell.
 * - Timing tuned for 72 MHz and optimized build.
 *
 * Variables:
 * - none.
 */
static inline __attribute__((always_inline)) void ws_send_bit_1(void)
{
    ws_high();
    __asm volatile (
        ".rept 34\n\t"
        "nop\n\t"
        ".endr\n\t"
    );
    ws_low();
    __asm volatile (
        ".rept 14\n\t"
        "nop\n\t"
        ".endr\n\t"
    );
}

/*
 * Function: ws_send_bit_0
 * -----------------------
 * Send logic '0' to WS2812.
 *
 * Method:
 * - Keeps line high shorter, then low longer.
 * - Timing tuned for 72 MHz and optimized build.
 *
 * Variables:
 * - none.
 */
static inline __attribute__((always_inline)) void ws_send_bit_0(void)
{
    ws_high();
    __asm volatile (
        ".rept 10\n\t"
        "nop\n\t"
        ".endr\n\t"
    );
    ws_low();
    __asm volatile (
        ".rept 32\n\t"
        "nop\n\t"
        ".endr\n\t"
    );
}

/*
 * Function: ws_send_byte
 * ----------------------
 * Send one byte to WS2812, MSB first.
 *
 * Method:
 * - Iterates from bit 7 to bit 0.
 *
 * Variables:
 * - value: byte to send.
 * - mask: current bit mask.
 */
static void ws_send_byte(uint8_t value)
{
    uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        if ((value & mask) != 0U)
        {
            ws_send_bit_1();
        }
        else
        {
            ws_send_bit_0();
        }
    }
}

/*
 * Function: xy_to_index
 * ---------------------
 * Convert visual XY coordinates to physical LED chain index.
 *
 * Method:
 * - Assumes column-wise serpentine panel wiring.
 * - Origin is bottom-left.
 * - Even columns run bottom-to-top.
 * - Odd columns run top-to-bottom.
 *
 * Variables:
 * - x: horizontal coordinate 0..7.
 * - y: vertical coordinate 0..7, where 0 is bottom.
 *
 * Returns:
 * - Physical LED chain index.
 *
 * Notes:
 * - If the image appears mirrored or rotated, tweak this function.
 */
static uint16_t xy_to_index(uint8_t x, uint8_t y)
{
    if ((x & 1U) == 0U)
    {
        return (uint16_t)(x * WS2812_PANEL_H + y);
    }
    else
    {
        return (uint16_t)(
            x * WS2812_PANEL_H + ((WS2812_PANEL_H - 1U) - y)
        );
    }
}

/*
 * Function: display_ws2812_init
 * -----------------------------
 * Initialize WS2812 output pin and clear the panel.
 *
 * Method:
 * - Enables GPIOB clock.
 * - Configures PB8 as push-pull output at 50 MHz.
 * - Clears local pixel buffer.
 * - Sends initial all-black frame.
 *
 * Variables:
 * - none.
 */
void display_ws2812_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    /*
     * PB8 -> CRH bits [3:0]
     * MODE8 = 11 (output 50 MHz)
     * CNF8  = 00 (general purpose push-pull)
     */
    GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
    GPIOB->CRH |= (GPIO_CRH_MODE8_0 | GPIO_CRH_MODE8_1);

    ws_low();
    display_ws2812_clear();
    display_ws2812_show();
}

/*
 * Function: display_ws2812_clear
 * ------------------------------
 * Clear the whole local WS2812 frame buffer.
 *
 * Method:
 * - Fills the GRB buffer with zeros.
 *
 * Variables:
 * - none.
 */
void display_ws2812_clear(void)
{
    memset(ws_pixels, 0, sizeof(ws_pixels));
}

/*
 * Function: display_ws2812_show
 * -----------------------------
 * Push the whole GRB frame buffer to the panel.
 *
 * Method:
 * - Temporarily disables interrupts so protocol timing is stable.
 * - Sends all pixel bytes.
 * - Holds line low long enough for latch/reset.
 *
 * Variables:
 * - i: byte index.
 */
void display_ws2812_show(void)
{
    uint32_t i;

    __disable_irq();

    for (i = 0U; i < (WS2812_LED_COUNT * 3U); i++)
    {
        ws_send_byte(ws_pixels[i]);
    }

    ws_low();

    /*
     * WS2812 latch/reset low period.
     */
    delay_cycles(6000U);

    __enable_irq();
}

/*
 * Function: display_ws2812_set_pixel_rgb
 * --------------------------------------
 * Set one visual pixel using RGB input values.
 *
 * Method:
 * - Converts visual XY to physical index.
 * - Stores data in GRB byte order expected by WS2812.
 *
 * Variables:
 * - x: visual x coordinate.
 * - y: visual y coordinate.
 * - r: red channel.
 * - g: green channel.
 * - b: blue channel.
 * - index: physical LED index.
 * - base: byte offset in ws_pixels.
 */
void display_ws2812_set_pixel_rgb(
    uint8_t x,
    uint8_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    uint16_t index;
    uint32_t base;

    if (x >= WS2812_PANEL_W || y >= WS2812_PANEL_H)
    {
        return;
    }

    index = xy_to_index(x, y);
    base = (uint32_t)index * 3U;

    ws_pixels[base + 0U] = g;
    ws_pixels[base + 1U] = r;
    ws_pixels[base + 2U] = b;
}

/*
 * Function: display_ws2812_fill_rgb
 * ---------------------------------
 * Fill the entire local frame buffer with one RGB color.
 *
 * Method:
 * - Iterates over all visual pixels and writes the same color.
 *
 * Variables:
 * - r: red channel.
 * - g: green channel.
 * - b: blue channel.
 * - x: x coordinate.
 * - y: y coordinate.
 */
void display_ws2812_fill_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t x;
    uint8_t y;

    for (y = 0U; y < WS2812_PANEL_H; y++)
    {
        for (x = 0U; x < WS2812_PANEL_W; x++)
        {
            display_ws2812_set_pixel_rgb(x, y, r, g, b);
        }
    }
}

/*
 * Function: display_ws2812_width
 * ------------------------------
 * Return panel width in pixels.
 *
 * Method:
 * - Returns compile-time constant.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - Panel width.
 */
uint8_t display_ws2812_width(void)
{
    return (uint8_t)WS2812_PANEL_W;
}

/*
 * Function: display_ws2812_height
 * -------------------------------
 * Return panel height in pixels.
 *
 * Method:
 * - Returns compile-time constant.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - Panel height.
 */
uint8_t display_ws2812_height(void)
{
    return (uint8_t)WS2812_PANEL_H;
}