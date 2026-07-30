#ifndef NULL_PARSER_H
#define NULL_PARSER_H

#include <stdint.h>

#define PARSER_MAX_COMMAND_LENGTH 32
#define PARSER_MAX_ARGS           16
#define PARSER_MAX_ARG_LENGTH     64

typedef struct
{
    char Command[PARSER_MAX_COMMAND_LENGTH];

    char Args[PARSER_MAX_ARGS][PARSER_MAX_ARG_LENGTH];

    uint32_t ArgCount;

} PARSED_COMMAND;

void ParserParse(
    const char *Input,
    PARSED_COMMAND *Output
);

#endif