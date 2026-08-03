#include "shell.h"
#include "drivers/input/keyboard/keyboard.h"
#include "kernel/terminal/terminal.h"
#include "lib/string/string.h"
#include "dispatcher.h"
#include "parser.h"
#include "editor/editor.h"

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;
uint32_t PromColor = 0xFFFFFFFF;
uint32_t BackSpaceColor = 0x00000000;

static char Command[SHELL_MAX_LENGTH];
static uint32_t CommandLength;

#define SHELL_HISTORY_SIZE 16U
static char History[SHELL_HISTORY_SIZE][SHELL_MAX_LENGTH];
static char HistoryDraft[SHELL_MAX_LENGTH];
static uint32_t HistoryCount;
static uint32_t HistoryPosition;
static bool HistoryBrowsing;

static void ShellReplaceCommand(const char *Replacement)
{
    while (CommandLength > 0)
    {
        TerminalBackspace32(BackSpaceColor);
        CommandLength--;
    }
    strcpy(Command, Replacement);
    CommandLength = (uint32_t)strlen(Command);
    TerminalWrite32(Command, PromColor);
}

static void ShellSaveHistory(void)
{
    if (CommandLength == 0) return;
    if (HistoryCount < SHELL_HISTORY_SIZE)
        strcpy(History[HistoryCount++], Command);
    else
    {
        for (uint32_t Index = 1; Index < SHELL_HISTORY_SIZE; Index++)
            strcpy(History[Index - 1U], History[Index]);
        strcpy(History[SHELL_HISTORY_SIZE - 1U], Command);
    }
    HistoryPosition = HistoryCount;
    HistoryBrowsing = false;
}

static void ShellHistoryUp(void)
{
    if (HistoryCount == 0) return;
    if (!HistoryBrowsing)
    {
        strcpy(HistoryDraft, Command);
        HistoryPosition = HistoryCount;
        HistoryBrowsing = true;
    }
    if (HistoryPosition > 0)
        ShellReplaceCommand(History[--HistoryPosition]);
}

static void ShellHistoryDown(void)
{
    if (!HistoryBrowsing) return;
    if (HistoryPosition + 1U < HistoryCount)
    {
        ShellReplaceCommand(History[++HistoryPosition]);
        return;
    }
    HistoryBrowsing = false;
    HistoryPosition = HistoryCount;
    ShellReplaceCommand(HistoryDraft);
}

void ShellInitialize(void)
{
    CommandLength = 0;
    HistoryCount = 0;
    HistoryPosition = 0;
    HistoryBrowsing = false;
    Command[0] = '\0';
    TerminalWrite32(PROMPT, PromColor);
}

void ShellUpdate(void)
{
    while (KbdAvailable())
    {
        uint8_t Key = KbdReadKey();
        char Character = (char)Key;

        if (EditorIsActive())
        {
            EditorHandleKey(Character, KbdGetState().Ctrl);
            if (!EditorIsActive())
            {
                CommandLength = 0;
                Command[0] = '\0';
                TerminalWrite32(PROMPT, PromColor);
            }
            continue;
        }

        if (Key == KBD_KEY_UP) { ShellHistoryUp(); continue; }
        if (Key == KBD_KEY_DOWN) { ShellHistoryDown(); continue; }

        if (Character == '\n')
        {
            TerminalPutChar32('\n', PromColor);
            ShellSaveHistory();
            Command[CommandLength] = '\0';

            PARSED_COMMAND Parsed;
            ParserParse(Command, &Parsed);
            DispatcherDispatch(&Parsed);

            CommandLength = 0;
            Command[0] = '\0';
            TerminalWrite32(PROMPT, PromColor);
            continue;
        }

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

        if (CommandLength >= SHELL_MAX_LENGTH - 1U)
        {
            continue;
        }

        Command[CommandLength++] = Character;
        TerminalPutChar32(Character, PromColor);
    }
}
