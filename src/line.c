#include "line.h"
#include "font5x8.h"
#include "display_max7219.h"

#define DEFAULT_SCROLL_STEP_MS 90U
#define MIN_SCROLL_STEP_MS 20U
#define MAX_SCROLL_STEP_MS 1000U

#define MAX_MESSAGE_LEN 96U
#define MAX_MESSAGE_COLS 768U

static uint8_t message_cols[MAX_MESSAGE_COLS];
static uint16_t message_width = 0U;
static uint16_t scroll_pos = 0U;
static uint16_t scroll_cycle = 0U;
static uint32_t scroll_step_ms = DEFAULT_SCROLL_STEP_MS;
static uint32_t scroll_paused = 0U;

static char current_text[MAX_MESSAGE_LEN] = "This is fine...";


/*
 * Function: sanitize_ascii_message
 * --------------------------------
 * Replace all non-printable ASCII characters with spaces.
 *
 * Method:
 * - Walk through the zero-terminated message buffer.
 * - Keep only printable ASCII in FONT5X8 range.
 * - Replace anything else with a space.
 *
 * Variables:
 * - s: mutable message buffer.
 * - i: character index.
 * - c: current byte value.
 */
static void sanitize_ascii_message(char *s)
{
    uint32_t i;
    uint8_t c;

    for (i = 0U; s[i] != '\0'; i++)
    {
        c = (uint8_t)s[i];

        if (c < FONT5X8_FIRST_CHAR || c > FONT5X8_LAST_CHAR)
        {
            s[i] = ' ';
        }
    }
}


/*
 * Function: build_message
 * -----------------------
 * Convert current_text into a stream of 5x8 font columns.
 *
 * Method:
 * - For each character, fetch its 5-column glyph.
 * - Append all 5 glyph columns to message_cols.
 * - Add one blank spacer column after each character.
 * - If the string is empty, keep a single blank column.
 *
 * Variables:
 * - out: write index in message_cols.
 * - i: character index in current_text.
 * - col: column index in one glyph.
 * - g: pointer to 5-column glyph data.
 */
static void build_message(void)
{
    uint16_t out = 0U;
    uint16_t i;
    uint8_t col;
    const uint8_t *g;

    for (i = 0U; current_text[i] != '\0'; i++)
    {
        g = font5x8_get(current_text[i]);

        for (col = 0U; col < FONT5X8_WIDTH; col++)
        {
            if (out < MAX_MESSAGE_COLS)
            {
                message_cols[out++] = g[col];
            }
        }

        if (out < MAX_MESSAGE_COLS)
        {
            message_cols[out++] = 0x00U;
        }
    }

    if (out == 0U)
    {
        message_cols[out++] = 0x00U;
    }

    message_width = out;
    scroll_cycle = (uint16_t)(message_width + DISPLAY_W);
}


/*
 * Function: render_scroll
 * -----------------------
 * Render the current scroll position into the MAX7219 framebuffer.
 *
 * Method:
 * - Clear the display framebuffer.
 * - For each visible screen column, map it to one source column
 *   from message_cols according to the current scroll position.
 * - For each set bit in the source column, light one display pixel.
 * - Use DISPLAY_H to cover all 8 vertical pixels.
 *
 * Variables:
 * - scroll: current scroll offset in columns.
 * - sx: visible X position on the display.
 * - src_index: source column index in message_cols.
 * - colbits: one source column bitmap.
 * - y: bit index inside the source column.
 */
static void render_scroll(uint16_t scroll)
{
    uint16_t sx;
    int32_t src_index;
    uint8_t colbits;
    uint8_t y;

    frame_clear();

    for (sx = 0U; sx < DISPLAY_W; sx++)
    {
        src_index = (int32_t)sx + (int32_t)scroll - (int32_t)DISPLAY_W;

        if (src_index < 0 || src_index >= (int32_t)message_width)
        {
            continue;
        }

        colbits = message_cols[src_index];

        for (y = 0U; y < DISPLAY_H; y++)
        {
            if ((colbits & (uint8_t)(1U << y)) != 0U)
            {
                set_pixel_xy((uint8_t)sx, y);
            }
        }
    }
}

/*
 * Function: line_init
 * -------------------
 * Initialize running line state and build the initial message.
 *
 * Method:
 * - Sanitize current_text to printable ASCII.
 * - Build message column stream.
 * - Reset scroll position and timing state.
 * - Start in unpaused mode with default speed.
 *
 * Variables:
 * - none.
 */
void line_init(void)
{
    scroll_step_ms = DEFAULT_SCROLL_STEP_MS;
    scroll_paused = 0U;
    scroll_pos = 0U;

    sanitize_ascii_message(current_text);
    build_message();
}

/*
 * Function: line_step
 * -------------------
 * Advance running line by one scheduled step.
 *
 * Method:
 * - Returns immediately if line is paused.
 * - Renders current scroll position.
 * - Flushes framebuffer to display.
 * - Advances scroll position and wraps at scroll_cycle.
 *
 * Variables:
 * - none.
 */
void line_step(void)
{
    if (scroll_paused != 0U)
    {
        return;
    }

    render_scroll(scroll_pos);
    frame_flush();

    scroll_pos++;
    if (scroll_pos >= scroll_cycle)
    {
        scroll_pos = 0U;
    }
}

/*
 * Function: line_get * Return current running-line period in milliseconds. * Function: line_get_period_ms
 *
 * Method:
 * - Returns current scroll period configured by line_set_speed().
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - Current running-line step period in milliseconds.
 */
uint32_t line_get_period_ms(void)
{
    return scroll_step_ms;
}

/*
 * Function: line_set_text
 * -----------------------
 * Replace the current running line text.
 *
 * Method:
 * - Copy the new zero-terminated string into current_text.
 * - Truncate to MAX_MESSAGE_LEN - 1 bytes.
 * - Sanitize to printable ASCII only.
 * - Rebuild column stream.
 * - Restart scrolling from the beginning.
 *
 * Variables:
 * - msg: new message string.
 * - i: copy index.
 */
void line_set_text(const char *msg)
{
    uint32_t i = 0U;

    while (i < (MAX_MESSAGE_LEN - 1U) && msg[i] != '\0')
    {
        current_text[i] = msg[i];
        i++;
    }

    current_text[i] = '\0';

    sanitize_ascii_message(current_text);
    build_message();

    scroll_pos = 0U;
}

/*
 * Function: line_pause
 * --------------------
 * Pause running line animation.
 *
 * Method:
 * - Set pause flag.
 *
 * Variables:
 * - none.
 */
void line_pause(void)
{
    scroll_paused = 1U;
}

/*
 * Function: line_resume
 * ---------------------
 * Resume running line animation.
 *
 * Method:
 * - Clear pause flag.
 *
 * Variables:
 * - none.
 */
void line_resume(void)
{
    scroll_paused = 0U;
}

/*
 * Function: line_set_speed
 * ------------------------
 * Set running line scroll period in milliseconds.
 *
 * Method:
 * - Clamp input to [MIN_SCROLL_STEP_MS, MAX_SCROLL_STEP_MS].
 * - Store the clamped period.
 *
 * Variables:
 * - ms: requested scroll period in milliseconds.
 */
void line_set_speed(uint32_t ms)
{
    if (ms < MIN_SCROLL_STEP_MS)
    {
        ms = MIN_SCROLL_STEP_MS;
    }

    if (ms > MAX_SCROLL_STEP_MS)
    {
        ms = MAX_SCROLL_STEP_MS;
    }

    scroll_step_ms = ms;
}

/*
 * Function: line_is_paused
 * ------------------------
 * Report whether the running line is currently paused.
 *
 * Method:
 * - Return internal pause flag.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - 0U when running.
 * - non-zero when paused.
 */
uint32_t line_is_paused(void)
{
    return scroll_paused;
}
