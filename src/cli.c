#include <stdint.h>
#include <string.h>

#include "cli.h"
#include "usb_cdc.h"

#define RX_LINE_LEN 128U

static char rx_line[RX_LINE_LEN];
static uint32_t rx_len = 0U;
static uint32_t cmd_ready = 0U;
static cli_command_t pending_cmd;

/*
 * Function: parse_u32_decimal
 * ---------------------------
 * Parse a zero-terminated decimal string into uint32_t.
 *
 * Method:
 * - Validates that all characters are decimal digits.
 * - Accumulates the numeric value in base 10.
 *
 * Variables:
 * - s: input zero-terminated decimal string.
 * - out_value: output pointer for parsed value.
 * - value: accumulated numeric result.
 * - i: string index.
 * - c: current character.
 *
 * Returns:
 * - 1 when parsing succeeds.
 * - 0 when input is empty or contains non-digits.
 */
static int parse_u32_decimal(const char *s, uint32_t *out_value)
{
    uint32_t value = 0U;
    uint32_t i = 0U;
    char c;

    if (s[0] == '\0')
    {
        return 0;
    }

    while ((c = s[i]) != '\0')
    {
        if ((c < '0') || (c > '9'))
        {
            return 0;
        }

        value = (value * 10U) + (uint32_t)(c - '0');
        i++;
    }

    *out_value = value;
    return 1;
}

/*
 * Function: cli_set_invalid
 * -------------------------
 * Mark the pending command as invalid.
 *
 * Method:
 * - Clears text and numeric value.
 * - Sets command type to CLI_CMD_INVALID.
 *
 * Variables:
 * - none.
 */
static void cli_set_invalid(void)
{
    pending_cmd.type = CLI_CMD_INVALID;
    pending_cmd.text[0] = '\0';
    pending_cmd.value = 0U;
}

/*
 * Function: cli_parse_line
 * ------------------------
 * Decode one completed command line into pending_cmd.
 *
 * Method:
 * - Recognizes:
 *   - print"message"
 *   - pause
 *   - resume
 *   - speed"ms"
 *   - intensity"value"
 *   - run snake
 *   - run line
 *   - help
 * - On syntax/format error sets CLI_CMD_INVALID.
 *
 * Variables:
 * - line: zero-terminated command line.
 * - inner: temporary text/number buffer.
 * - start: pointer to data after opening quote.
 * - end: pointer to closing quote.
 * - len: length of quoted payload.
 * - i: copy index.
 * - value: parsed numeric value.
 */
static void cli_parse_line(const char *line)
{
    char inner[CLI_MAX_TEXT_LEN];
    const char *start;
    const char *end;
    uint32_t len;
    uint32_t i;
    uint32_t value;

    pending_cmd.type = CLI_CMD_NONE;
    pending_cmd.text[0] = '\0';
    pending_cmd.value = 0U;

    if (strcmp(line, "pause") == 0)
    {
        pending_cmd.type = CLI_CMD_PAUSE;
        return;
    }

    if (strcmp(line, "resume") == 0)
    {
        pending_cmd.type = CLI_CMD_RESUME;
        return;
    }

    if (strcmp(line, "help") == 0)
    {
        pending_cmd.type = CLI_CMD_HELP;
        return;
    }

    if (strcmp(line, "run snake") == 0)
    {
        pending_cmd.type = CLI_CMD_RUN_SNAKE;
        return;
    }

    if (strcmp(line, "run line") == 0)
    {
        pending_cmd.type = CLI_CMD_RUN_LINE;
        return;
    }

    if (strncmp(line, "print\"", 6) == 0)
    {
        start = &line[6];
        end = strrchr(start, '\"');

        if (end == NULL)
        {
            cli_set_invalid();
            return;
        }

        len = (uint32_t)(end - start);
        if (len >= CLI_MAX_TEXT_LEN)
        {
            len = CLI_MAX_TEXT_LEN - 1U;
        }

        for (i = 0U; i < len; i++)
        {
            pending_cmd.text[i] = start[i];
        }

        pending_cmd.text[len] = '\0';
        pending_cmd.type = CLI_CMD_PRINT;
        return;
    }

    if (strncmp(line, "speed\"", 6) == 0)
    {
        start = &line[6];
        end = strrchr(start, '\"');

        if (end == NULL)
        {
            cli_set_invalid();
            return;
        }

        len = (uint32_t)(end - start);
        if (len == 0U || len >= sizeof(inner))
        {
            cli_set_invalid();
            return;
        }

        for (i = 0U; i < len; i++)
        {
            inner[i] = start[i];
        }

        inner[len] = '\0';

        if (!parse_u32_decimal(inner, &value))
        {
            cli_set_invalid();
            return;
        }

        pending_cmd.type = CLI_CMD_SPEED;
        pending_cmd.value = value;
        return;
    }

    if (strncmp(line, "intensity\"", 10) == 0)
    {
        start = &line[10];
        end = strrchr(start, '\"');

        if (end == NULL)
        {
            cli_set_invalid();
            return;
        }

        len = (uint32_t)(end - start);
        if (len == 0U || len >= sizeof(inner))
        {
            cli_set_invalid();
            return;
        }

        for (i = 0U; i < len; i++)
        {
            inner[i] = start[i];
        }

        inner[len] = '\0';

        if (!parse_u32_decimal(inner, &value))
        {
            cli_set_invalid();
            return;
        }

        pending_cmd.type = CLI_CMD_INTENSITY;
        pending_cmd.value = value;
        return;
    }

    cli_set_invalid();
}

/*
 * Function: cli_init
 * ------------------
 * Reset CLI parser state.
 *
 * Method:
 * - Clears input line buffer.
 * - Clears ready flag and pending command.
 *
 * Variables:
 * - none.
 */
void cli_init(void)
{
    rx_len = 0U;
    cmd_ready = 0U;
    rx_line[0] = '\0';
    pending_cmd.type = CLI_CMD_NONE;
    pending_cmd.text[0] = '\0';
    pending_cmd.value = 0U;
}

/*
 * Function: cli_on_char
 * ---------------------
 * Feed one received character into line-mode CLI parser.
 *
 * Method:
 * - Echoes typed characters in line mode.
 * - Handles CR/LF, backspace and normal input.
 * - On CR: finalizes the line, parses it, and marks command ready.
 *
 * Variables:
 * - ch: input character.
 */
void cli_on_char(char ch)
{
    if (ch == '\r')
    {
        usb_cdc_write_bytes("\r\n", 2U);

        if (rx_len < RX_LINE_LEN)
        {
            rx_line[rx_len] = '\0';
        }
        else
        {
            rx_line[RX_LINE_LEN - 1U] = '\0';
        }

        cli_parse_line(rx_line);
        cmd_ready = 1U;
        rx_len = 0U;
        return;
    }

    if (ch == '\n')
    {
        return;
    }

    if (ch == '\b' || ch == 0x7FU)
    {
        if (rx_len > 0U)
        {
            rx_len--;
            usb_cdc_echo_char(ch);
        }
        return;
    }

    if (rx_len < (RX_LINE_LEN - 1U))
    {
        rx_line[rx_len++] = ch;
        usb_cdc_echo_char(ch);
    }
}

/*
 * Function: cli_command_ready
 * ---------------------------
 * Report whether a parsed command is available.
 *
 * Method:
 * - Returns internal ready flag.
 *
 * Variables:
 * - none.
 *
 * Returns:
 * - 0 when no command is pending.
 * - non-zero when a command is ready to be popped.
 */
uint32_t cli_command_ready(void)
{
    return cmd_ready;
}

/*
 * Function: cli_pop_command
 * -------------------------
 * Copy pending command to caller and clear ready flag.
 *
 * Method:
 * - Writes pending_cmd into caller-provided structure.
 * - Clears internal ready flag.
 * - Resets pending command to CLI_CMD_NONE.
 *
 * Variables:
 * - out_cmd: destination structure pointer.
 */
void cli_pop_command(cli_command_t *out_cmd)
{
    if (out_cmd == 0)
    {
        return;
    }

    *out_cmd = pending_cmd;
    cmd_ready = 0U;
    pending_cmd.type = CLI_CMD_NONE;
    pending_cmd.text[0] = '\0';
    pending_cmd.value = 0U;
}
