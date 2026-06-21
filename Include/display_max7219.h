#ifndef DISPLAY_MAX7219_H
#define DISPLAY_MAX7219_H

#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#define MODULE_COUNT 4U
#define DISPLAY_W 32U
#define DISPLAY_H 8U

#define MODULE0_IS_RIGHTMOST 1U
#define LOCAL_X_LSB_IS_RIGHTMOST 0U
#define ROW0_IS_TOP 1U

#define INTENSITY_VALUE 0x03U

void max7219_init(void);
void spi2_setup(void);
void max7219_set_intensity(uint8_t value);
void frame_clear(void);
void frame_flush(void);
void set_pixel_xy(uint8_t x, uint8_t y);

#endif