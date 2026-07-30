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


    while (Input[InputIndex] != '\0' && Input[InputIndex] != ' ')
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


    while (Input[InputIndex] != '\0')
    {
        uint32_t ArgumentIndex = 0;


        while (Input[InputIndex] != '\0' && Input[InputIndex] != ' ')
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