#include <stdint.h>

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "line.h"
#include "display_max7219.h"
#include "snake.h"
#include "usb_cdc.h"
#include "cli.h"

typedef enum
{
    MODE_SCROLL = 0,
    MODE_SNAKE = 1
} app_mode_t;

static volatile uint32_t sys_ms = 0U;
static app_mode_t current_mode = MODE_SCROLL;

/*
 * Function: delay_cycles
 * ----------------------
 * Small busy-loop delay used for USB re-enumeration timing.
 *
 * Method:
 * - Executes NOP instructions in a countdown loop.
 *
 * Variables:
 * - cycles: number of loop iterations to execute.
 */
static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--)
    {
        __asm volatile ("nop");
    }
}

/*
 * Function: SysTick_Handler
 * -------------------------
 * 1 ms system tick interrupt handler.
 *
 * Method:
 * - Increments a global millisecond counter.
 *
 * Variables:
 * - sys_ms: global millisecond tick counter.
 */
void SysTick_Handler(void)
{
    sys_ms++;
}

/*
 * Function: systick_setup
 * -----------------------
 * Configure SysTick for a 1 ms tick at 72 MHz CPU clock.
 *
 * Method:
 * - Uses AHB as the SysTick clock source.
 * - Reload value is set for 1 ms period.
 *
 * Variables:
 * - none.
 */
static void systick_setup(void)
{
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(72000U - 1U);
    systick_interrupt_enable();
    systick_counter_enable();
}

/*
 * Function: usb_reenumerate
 * -------------------------
 * Force USB disconnect/reconnect on boards that need it.
 *
 * Method:
 * - Drives PA12 low temporarily.
 * - Waits briefly.
 * - Releases PA12 back to floating input.
 *
 * Variables:
 * - none.
 */
static void usb_reenumerate(void)
{
    gpio_set_mode(
        GPIOA,
        GPIO_MODE_OUTPUT_2_MHZ,
        GPIO_CNF_OUTPUT_OPENDRAIN,
        GPIO12
    );

    gpio_clear(GPIOA, GPIO12);
    delay_cycles(800000U);

    gpio_set_mode(
        GPIOA,
        GPIO_MODE_INPUT,
        GPIO_CNF_INPUT_FLOAT,
        GPIO12
    );
}

/*
 * Function: clock_setup
 * ---------------------
 * Configure MCU core clock and enable required peripherals.
 *
 * Method:
 * - Sets 72 MHz PLL clock from 8 MHz HSE.
 * - Enables GPIOA, GPIOB, GPIOC, AFIO, SPI2 and USB clocks.
 *
 * Variables:
 * - none.
 */
static void clock_setup(void)
{
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_AFIO);
    rcc_periph_clock_enable(RCC_SPI2);
    rcc_periph_clock_enable(RCC_USB);
}

/*
 * Function: gpio_setup
 * --------------------
 * Configure GPIO used by MAX7219 and onboard LED.
 *
 * Method:
 * - PB13 and PB15 are SPI2 alternate-function push-pull.
 * - PB12 is MAX7219 chip-select output.
 * - PC13 is onboard LED output.
 *
 * Variables:
 * - none.
 */
static void gpio_setup(void)
{
    gpio_set_mode(
        GPIOB,
        GPIO_MODE_OUTPUT_50_MHZ,
        GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
        GPIO13 | GPIO15
    );

    gpio_set_mode(
        GPIOB,
        GPIO_MODE_OUTPUT_50_MHZ,
        GPIO_CNF_OUTPUT_PUSHPULL,
        GPIO12
    );

    gpio_set_mode(
        GPIOC,
        GPIO_MODE_OUTPUT_2_MHZ,
        GPIO_CNF_OUTPUT_PUSHPULL,
        GPIO13
    );

    gpio_set(GPIOB, GPIO12);
    gpio_set(GPIOC, GPIO13);
}

/*
 * Function: app_exit_snake_to_line
 * --------------------------------
 * Leave snake mode and return to running-line mode.
 *
 * Method:
 * - Switches application mode.
 * - Resumes line animation.
 * - Clears pending snake exit request.
 *
 * Variables:
 * - none.
 */
static void app_exit_snake_to_line(void)
{
    current_mode = MODE_SCROLL;
    line_resume();
    snake_clear_exit_requested();
}

/*
 * Function: send_help
 * -------------------
 * Send help text for all supported commands and snake keys.
 *
 * Method:
 * - Writes a fixed help message through USB CDC.
 *
 * Variables:
 * - none.
 */
static void send_help(void)
{
    usb_cdc_write_text("Commands:\r\n");
    usb_cdc_write_text("  print\"message\"\r\n");
    usb_cdc_write_text("  pause\r\n");
    usb_cdc_write_text("  resume\r\n");
    usb_cdc_write_text("  speed\"ms\"\r\n");
    usb_cdc_write_text("  intensity\"0..15\"\r\n");
    usb_cdc_write_text("  run snake\r\n");
    usb_cdc_write_text("  run line\r\n");
    usb_cdc_write_text("  help\r\n");
    usb_cdc_write_text("Snake keys:\r\n");
    usb_cdc_write_text("  W A S D  move\r\n");
    usb_cdc_write_text("  P        pause/resume\r\n");
    usb_cdc_write_text("  R        restart\r\n");
    usb_cdc_write_text("  Q        quit to running line ");
    usb_cdc_write_text("(paused only)\r\n");
}

/*
 * Function: app_execute_cli_command
 * ---------------------------------
 * Execute one decoded CLI command.
 *
 * Method:
 * - Switches on command type.
 * - Dispatches work to line/snake/display modules.
 * - Sends command result text over USB CDC.
 *
 * Variables:
 * - cmd: decoded command to execute.
 * - intensity: clamped display intensity value.
 */
static void app_execute_cli_command(const cli_command_t *cmd)
{
    uint8_t intensity;

    switch (cmd->type)
    {
        case CLI_CMD_PRINT:
            line_set_text(cmd->text);
            current_mode = MODE_SCROLL;
            line_resume();
            usb_cdc_write_text("OK print\r\n");
            break;

        case CLI_CMD_PAUSE:
            line_pause();
            usb_cdc_write_text("OK paused\r\n");
            break;

        case CLI_CMD_RESUME:
            line_resume();
            usb_cdc_write_text("OK resumed\r\n");
            break;

        case CLI_CMD_SPEED:
            line_set_speed(cmd->value);
            usb_cdc_write_text("OK speed\r\n");
            break;

        case CLI_CMD_INTENSITY:
            intensity = (cmd->value > 15U) ? 15U : (uint8_t)cmd->value;
            max7219_set_intensity(intensity);
            usb_cdc_write_text("OK intensity\r\n");
            break;

        case CLI_CMD_RUN_SNAKE:
            current_mode = MODE_SNAKE;
            snake_reset();
            snake_render();
            frame_flush();
            usb_cdc_write_text("OK snake\r\n");
            break;

        case CLI_CMD_RUN_LINE:
            current_mode = MODE_SCROLL;
            line_resume();
            usb_cdc_write_text("OK line\r\n");
            break;

        case CLI_CMD_HELP:
            send_help();
            break;

        case CLI_CMD_INVALID:
        default:
            usb_cdc_write_text("ERR unknown command\r\n");
            break;
    }
}

/*
 * Function: app_on_rx_char
 * ------------------------
 * Route one incoming USB CDC byte to the correct subsystem.
 *
 * Method:
 * - In snake mode:
 *   - accepts only gameplay keys,
 *   - ignores all others,
 *   - performs no echo.
 * - In running-line mode:
 *   - passes every byte to CLI parser.
 *
 * Variables:
 * - ch: received character.
 * - allowed_snake_key: non-zero when the byte is valid for snake.
 */
static void app_on_rx_char(char ch)
{
    uint32_t allowed_snake_key;

    if (current_mode == MODE_SNAKE)
    {
        allowed_snake_key =
            (ch == 'w') || (ch == 'W') ||
            (ch == 'a') || (ch == 'A') ||
            (ch == 's') || (ch == 'S') ||
            (ch == 'd') || (ch == 'D') ||
            (ch == 'p') || (ch == 'P') ||
            (ch == 'r') || (ch == 'R') ||
            (((ch == 'q') || (ch == 'Q')) &&
             (snake_is_paused() != 0U));

        if (allowed_snake_key != 0U)
        {
            snake_handle_key(ch);
        }

        return;
    }

    cli_on_char(ch);
}

/*
 * Function: main
 * --------------
 * Application entry point.
 *
 * Method:
 * - Initializes clocks, GPIO, display, USB CDC, line and snake modules.
 * - Polls USB CDC transport.
 * - Executes decoded CLI commands.
 * - Runs either running-line mode or snake mode.
 *
 * Variables:
 * - now_ms: current system time in milliseconds.
 * - cmd: decoded command popped from CLI module.
 */
int main(void)
{
    uint32_t now_ms;
    cli_command_t cmd;

    clock_setup();
    gpio_setup();
    spi2_setup();
    systick_setup();
    usb_reenumerate();
    max7219_init();

    usb_cdc_init();
    usb_cdc_set_rx_callback(app_on_rx_char);

    line_init();
    snake_init();
    cli_init();

    while (1)
    {
        usb_cdc_poll();

        if (cli_command_ready() != 0U)
        {
            cli_pop_command(&cmd);
            app_execute_cli_command(&cmd);
        }

        now_ms = sys_ms;

        if (current_mode == MODE_SCROLL)
        {
            line_tick(now_ms);
        }
        else
        {
            snake_tick(now_ms);
            snake_render();
            frame_flush();

            if (snake_exit_requested() != 0U)
            {
                app_exit_snake_to_line();
            }
        }
    }
}