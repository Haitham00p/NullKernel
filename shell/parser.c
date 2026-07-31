#include "parser.h"
#include <stdint.h>

void ParserParse(const char *Input, PARSED_COMMAND *Output)
{
    uint32_t InputIndex = 0;
    uint32_t CommandIndex = 0;

    Output->ArgCount = 0;

    Output->Command[0] = '\0';

    for (uint32_t i = 0; i < PARSER_MAX_ARGS; i++)
    {
        Output->Args[i][0] = '\0';
    }


    while (Input[InputIndex] != '\0' && Input[InputIndex] != ' ' &&
           CommandIndex < PARSER_MAX_COMMAND_LENGTH - 1U)
    {
        Output->Command[CommandIndex] = Input[InputIndex];

        CommandIndex++;
        InputIndex++;
    }

    Output->Command[CommandIndex] = '\0';


    while (Input[InputIndex] == ' ')
    {
        InputIndex++;
    }


    while (Input[InputIndex] != '\0' && Output->ArgCount < PARSER_MAX_ARGS)
    {
        uint32_t ArgumentIndex = 0;


        while (Input[InputIndex] != '\0' && Input[InputIndex] != ' ' &&
               ArgumentIndex < PARSER_MAX_ARG_LENGTH - 1U)
        {
            Output->Args[Output->ArgCount][ArgumentIndex] = Input[InputIndex];

            ArgumentIndex++;
            InputIndex++;
        }


        Output->Args[Output->ArgCount][ArgumentIndex] = '\0';

        Output->ArgCount++;


        while (Input[InputIndex] == ' ')
        {
            InputIndex++;
        }
    }
}
