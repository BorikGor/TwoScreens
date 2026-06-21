#include "display_max7219.h"

static uint8_t frame[MODULE_COUNT][8];

static void cs_low(void)
{
    gpio_clear(GPIOB, GPIO12);
}

static void cs_high(void)
{
    gpio_set(GPIOB, GPIO12);
}

static void spi2_write8(uint8_t value)
{
    spi_send(SPI2, value);
    (void)spi_read(SPI2);

    while ((SPI_SR(SPI2) & SPI_SR_BSY) != 0U)
    {
    }
}

static void max7219_shift_pair(uint8_t reg, uint8_t data)
{
    spi2_write8(reg);
    spi2_write8(data);
}

static void max7219_write_all(uint8_t reg, uint8_t data)
{
    uint8_t i;

    cs_low();

    for (i = 0U; i < MODULE_COUNT; i++)
    {
        max7219_shift_pair(reg, data);
    }

    cs_high();
}

void max7219_set_intensity(uint8_t value)
{
    if (value > 0x0FU)
    {
        value = 0x0FU;
    }

    max7219_write_all(0x0AU, value);
}

void max7219_init(void)
{
    uint8_t row;
    uint8_t pass;

    max7219_write_all(0x0FU, 0x00U);
    max7219_write_all(0x0CU, 0x00U);
    max7219_write_all(0x09U, 0x00U);
    max7219_write_all(0x0BU, 0x07U);
    max7219_set_intensity(INTENSITY_VALUE);
    for (pass = 0U; pass < 3U; pass++)
    {
        for (row = 1U; row <= 8U; row++)
        {
            max7219_write_all(row, 0x00U);
        }
    }

    max7219_write_all(0x0CU, 0x01U);
}

void spi2_setup(void)
{
    spi_disable(SPI2);
    spi_set_master_mode(SPI2);
    spi_set_baudrate_prescaler(SPI2, SPI_CR1_BAUDRATE_FPCLK_DIV_256);
    spi_set_standard_mode(SPI2, 0);
    spi_send_msb_first(SPI2);
    spi_enable_software_slave_management(SPI2);
    spi_set_nss_high(SPI2);
    spi_enable(SPI2);
}

void frame_clear(void)
{
    uint8_t m;
    uint8_t r;

    for (m = 0U; m < MODULE_COUNT; m++)
    {
        for (r = 0U; r < 8U; r++)
        {
            frame[m][r] = 0x00U;
        }
    }
}

void frame_flush(void)
{
    uint8_t row;
    int m;

    for (row = 0U; row < 8U; row++)
    {
        cs_low();

        for (m = (int)MODULE_COUNT - 1; m >= 0; m--)
        {
            max7219_shift_pair((uint8_t)(row + 1U), frame[m][row]);
        }

        cs_high();
    }
}

void set_pixel_xy(uint8_t x, uint8_t y)
{
    uint8_t module;
    uint8_t local_x;
    uint8_t row;
    uint8_t bit;

    if (x >= DISPLAY_W || y >= DISPLAY_H)
    {
        return;
    }

    if (MODULE0_IS_RIGHTMOST != 0U)
    {
        module = (uint8_t)((DISPLAY_W - 1U - x) / 8U);
    }
    else
    {
        module = (uint8_t)(x / 8U);
    }

    local_x = (uint8_t)(x % 8U);

    if (LOCAL_X_LSB_IS_RIGHTMOST != 0U)
    {
        bit = (uint8_t)(1U << local_x);
    }
    else
    {
        bit = (uint8_t)(1U << (7U - local_x));
    }

    if (ROW0_IS_TOP != 0U)
    {
        row = y;
    }
    else
    {
        row = (uint8_t)(7U - y);
    }

    frame[module][row] |= bit;
}
