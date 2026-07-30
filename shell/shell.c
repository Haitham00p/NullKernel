#include "shell.h"
#include "../drivers/keyboard.h"
#include "../terminal/terminal.h"
#include "../string/string.h"
#include "dispatcher.h"
#include "parser.h"

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;
uint32_t PromColor = 0xFFFFFFFF;
uint32_t BackSpaceColor = 0x0000000;

static char Command[SHELL_MAX_LENGTH];
static uint32_t CommandLength;


void ShellInitialize(void)
{
    CommandLength = 0;
    Command[0] = '\0';

    TerminalWrite32(PROMPT, PromColor);
}




void ShellUpdate(void)
{
    while (KbdAvailable())
    {
        char Character = KbdReadChar();
        
        if (Character == '\n')
        {
            TerminalPutChar32('\n', PromColor);


            Command[CommandLength] = '\0';

            PARSED_COMMAND cmd;
            ParserParse(Command, &cmd);
            DispatcherDispatch(&cmd);
            

            CommandLength = 0;
            Command[0] = '\0';


            TerminalWrite32(PROMPT, PromColor);


            continue;
        }

        

        /*
         * Backspace
         */
        if (Character == '\b')
        {
            if (CommandLength > 0)
            {
                CommandLength--;

                Command[CommandLength] = '\0';

                TerminalBackspace32(BackSpaceColor);
            }


            continue;
        }



        /*
         * Command buffer limit
         */
        if (CommandLength >= SHELL_MAX_LENGTH - 1)
        {
            continue;
        }



        /*
         * Add character
         */
        Command[CommandLength++] = Character;


        TerminalPutChar32(Character, PromColor);
    }
}
