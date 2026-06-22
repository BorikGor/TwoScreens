#include <stdint.h>

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "line.h"
#include "display_max7219.h"
#include "display_ws2812.h"
#include "usb_cdc.h"
#include "cli.h"
#include "snake.h"
#include "fire.h"

typedef enum
{
    MODE_SCROLL = 0,
    MODE_SNAKE = 1
} app_mode_t;

static volatile uint32_t sys_ms = 0U;
static app_mode_t current_mode = MODE_SCROLL;

/* Timer constants */
#define LED_TOGGLE_MS   500U
#define SNAKE_STEP_MS   180U
#define FIRE_STEP_MS    40U

static volatile uint8_t line_due = 0U;
static volatile uint8_t snake_due = 0U;
static volatile uint8_t fire_due = 0U;
static volatile uint8_t led_due = 0U;

static volatile uint32_t line_acc_ms = 0U;
static volatile uint32_t snake_acc_ms = 0U;
static volatile uint32_t fire_acc_ms = 0U;
static volatile uint32_t led_acc_ms = 0U;

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
 * 1 ms scheduler interrupt handler.
 *
 * Method:
 * - Increments global millisecond counter.
 * - Accumulates elapsed time for line, snake, fire and LED tasks.
 * - Sets due flags when a task period expires.
 *
 * Variables:
 * - sys_ms: global millisecond counter.
 * - line_acc_ms: running-line scheduler accumulator.
 * - snake_acc_ms: snake scheduler accumulator.
 * - fire_acc_ms: fire scheduler accumulator.
 * - led_acc_ms: LED scheduler accumulator.
 * - line_due, snake_due, fire_due, led_due: task due flags.
 */
void SysTick_Handler(void)
{
    sys_ms++;

    line_acc_ms++;
    snake_acc_ms++;
    fire_acc_ms++;
    led_acc_ms++;

    if (line_acc_ms >= line_get_period_ms())
    {
        line_acc_ms = 0U;
        line_due = 1U;
    }

    if (snake_acc_ms >= SNAKE_STEP_MS)
    {
        snake_acc_ms = 0U;
        snake_due = 1U;
    }

    if (fire_acc_ms >= FIRE_STEP_MS)
    {
        fire_acc_ms = 0U;
        fire_due = 1U;
    }

    if (led_acc_ms >= LED_TOGGLE_MS)
    {
        led_acc_ms = 0U;
        led_due = 1U;
    }
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
 * Function: led_toggle
 * --------------------
 * Toggle onboard LED state.
 *
 * Method:
 * - Toggles PC13 output state.
 *
 * Variables:
 * - none.
 */
static void led_toggle(void)
{
    gpio_toggle(GPIOC, GPIO13);
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
            usb_cdc_write_text("OK print\r\n");
            line_set_text(cmd->text);
            current_mode = MODE_SCROLL;
            line_resume();
            break;

        case CLI_CMD_PAUSE:
            usb_cdc_write_text("OK paused\r\n");
            line_pause();
            break;

        case CLI_CMD_RESUME:
            usb_cdc_write_text("OK resumed\r\n");
            line_resume();
            break;

        case CLI_CMD_SPEED:
            usb_cdc_write_text("OK speed\r\n");
            line_set_speed(cmd->value);
            break;

        case CLI_CMD_INTENSITY:
            usb_cdc_write_text("OK intensity\r\n");
            intensity = (cmd->value > 15U) ? 15U : (uint8_t)cmd->value;
            max7219_set_intensity(intensity);
            break;

        case CLI_CMD_RUN_SNAKE:
            current_mode = MODE_SNAKE;
            usb_cdc_write_text("OK snake\r\n");
            snake_reset();
            snake_render();
            frame_flush();
            snake_due = 0;
            break;

        case CLI_CMD_RUN_LINE:
            usb_cdc_write_text("OK line\r\n");
            current_mode = MODE_SCROLL;
            line_resume();
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
 - Initializes clocks, GPIO, displays, USB CDC, line, CLI, snake and fire. * Application entry point.
 * - Polls USB CDC transport continuously.
 * - Executes decoded CLI commands.
 * - Runs all periodic work from SysTick-driven due flags.
 *
 * Variables:
 * - cmd: decoded command popped from CLI module.
 */
int main(void)
{
    cli_command_t cmd;

    clock_setup();
    gpio_setup();
    spi2_setup();
    systick_setup();
    usb_reenumerate();
    max7219_init();
    display_ws2812_init();

    usb_cdc_init();
    usb_cdc_set_rx_callback(app_on_rx_char);

    line_init();
    cli_init();
    snake_init();
    fire_init();

    while (1)
    {
        usb_cdc_poll();

        if (cli_command_ready() != 0U)
        {
            cli_pop_command(&cmd);
            app_execute_cli_command(&cmd);
        }

        // now_ms = sys_ms;
        if(led_due !=0)
        {
            led_due = 0;
            led_toggle();
        }

        if(fire_due != 0)
        {
            fire_due = 0;
            fire_step();
        }

        if (current_mode == MODE_SCROLL)
        {
            if(line_due != 0)
            {
                line_due = 0;
                line_step();
            }
        }
        else
        {
            if(snake_due != 0)
            {
                snake_due = 0;
                snake_step();
                snake_render();
                frame_flush();
            }

            if (snake_exit_requested() != 0U)
            {
                app_exit_snake_to_line();
            }
        }
    }
}