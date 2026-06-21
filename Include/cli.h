#ifndef CLI_H
#define CLI_H

#include <stdint.h>

#define CLI_MAX_TEXT_LEN 96U

typedef enum
{
    CLI_CMD_NONE = 0,
    CLI_CMD_PRINT,
    CLI_CMD_PAUSE,
    CLI_CMD_RESUME,
    CLI_CMD_SPEED,
    CLI_CMD_INTENSITY,
    CLI_CMD_RUN_SNAKE,
    CLI_CMD_RUN_LINE,
    CLI_CMD_HELP,
    CLI_CMD_INVALID
} cli_cmd_type_t;

typedef struct
{
    cli_cmd_type_t type;
    char text[CLI_MAX_TEXT_LEN];
    uint32_t value;
} cli_command_t;

void cli_init(void);
void cli_on_char(char ch);
uint32_t cli_command_ready(void);
void cli_pop_command(cli_command_t *out_cmd);

#endif