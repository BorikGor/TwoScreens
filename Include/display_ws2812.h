#ifndef DISPLAY_WS2812_H
#define DISPLAY_WS2812_H

#include <stdint.h>

/*
 * Panel geometry.
 *
 * Method:
 * - 8x8 WS2812B panel.
 * - Visual origin is bottom-left.
 *
 * Variables:
 * - WS2812_PANEL_W: panel width in pixels.
 * - WS2812_PANEL_H: panel height in pixels.
 * - WS2812_LED_COUNT: total LED count.
 */
#define WS2812_PANEL_W       8U
#define WS2812_PANEL_H       8U
#define WS2812_LED_COUNT     (WS2812_PANEL_W * WS2812_PANEL_H)

/*
 * Function: display_ws2812_init
 * -----------------------------
 * Initialize WS2812 output pin and clear the panel.
 *
 * Method:
 * - Enables GPIOB clock.
 * - Configures PB8 as push-pull output.
 * - Clears local pixel buffer.
 * - Sends initial all-black frame.
 *
 * Variables:
 * - none.
 */
void display_ws2812_init(void);

/*
 * Function: display_ws2812_clear
 * ------------------------------
 * Clear the whole local WS2812 frame buffer.
 *
 * Method:
 * - Fills the internal GRB buffer with zeros.
 *
 * Variables:
 * - none.
 */
void display_ws2812_clear(void);

/*
 * Function: display_ws2812_show
 * -----------------------------
 * Send the whole local frame buffer to the WS2812 panel.
 *
 * Method:
 * - Transmits all stored GRB bytes.
 * - Uses timing tuned for 72 MHz STM32F103.
 *
 * Variables:
 * - none.
 */
void display_ws2812_show(void);

/*
 * Function: display_ws2812_set_pixel_rgb
 * --------------------------------------
 * Set one pixel in the local frame buffer using visual XY coordinates.
 *
 * Method:
 * - Converts visual XY to physical LED index.
 * - Stores values in GRB byte order.
 *
 * Variables:
 * - x: visual x coordinate.
 * - y: visual y coordinate.
 * - r: red channel.
 * - g: green channel.
 * - b: blue channel.
 */
void display_ws2812_set_pixel_rgb(
    uint8_t x,
    uint8_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

/*
 * Function: display_ws2812_fill_rgb
 * ---------------------------------
 * Fill the entire local frame buffer with one RGB color.
 *
 * Method:
 * - Iterates over all visual pixels.
 * - Writes identical RGB value to every pixel.
 *
 * Variables:
 * - r: red channel.
 * - g: green channel.
 * - b: blue channel.
 */
void display_ws2812_fill_rgb(uint8_t r, uint8_t g, uint8_t b);

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
uint8_t display_ws2812_width(void);

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
uint8_t display_ws2812_height(void);

#endif