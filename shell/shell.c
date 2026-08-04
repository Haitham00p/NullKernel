#include "shell.h"
#include "drivers/input/keyboard/keyboard.h"
#include "kernel/terminal/terminal.h"
#include "include/cursor.h"
#include "lib/string/string.h"
#include "dispatcher.h"
#include "parser.h"
#include "editor/editor.h"

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;
uint32_t PromColor = 0xFFFFFFFF;
uint32_t BackSpaceColor = 0x00000000;

static char Command[SHELL_MAX_LENGTH];
static uint32_t CommandLength = 0;
static uint32_t CommandPos = 0;

#define SHELL_HISTORY_SIZE 16U
static char History[SHELL_HISTORY_SIZE][SHELL_MAX_LENGTH];
static char HistoryDraft[SHELL_MAX_LENGTH];
static uint32_t HistoryCount = 0;
static uint32_t HistoryPosition = 0;
static bool HistoryBrowsing = false;

/**
 * @brief Reads base text color and applies channel math to generate
 * a harmonized, slightly shifted single color for the whole prompt.
 */
static uint32_t ComputePromptColor(uint32_t base) {
    uint32_t a = (base >> 24) ? ((base >> 24) & 0xFF) : 0xFF;
    uint32_t r = (base >> 16) & 0xFF;
    uint32_t g = (base >> 8) & 0xFF;
    uint32_t b = base & 0xFF;

    /* Channel math: scale R slightly, boost G/B channels to give sleek cyan/soft tint */
    uint32_t r_prompt = (r * 3) / 4;
    uint32_t g_prompt = ((g * 9) / 10) > 0xDF ? 0xFF : (((g * 9) / 10) + 0x20);
    uint32_t b_prompt = (b > 0xBF) ? 0xFF : (b + 0x40);

    return (a << 24) | (r_prompt << 16) | (g_prompt << 8) | b_prompt;
}

static void WritePrompt(void) {
    uint32_t prompt_color = ComputePromptColor(PromColor);
    TerminalWrite32(PROMPT, prompt_color);
}

static void RedrawCommandPrompt(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t cur_x = 0, cur_y = 0;
    TerminalGetCursor32(&cur_x, &cur_y);
    uint32_t prompt_start_col = (uint32_t)(strlen(PROMPT));

    /* Clear current prompt line content */
    TerminalSetCursor32(0, cur_y);
    WritePrompt();
    TerminalWrite32(Command, PromColor);
    
    /* Fill remaining line length with spaces to clean up deleted chars */
    for (uint32_t i = (uint32_t)strlen(PROMPT) + CommandLength; i < scr->cols; i++) {
        TerminalPutChar32(' ', PromColor);
    }

    /* Position cursor at active editing offset CommandPos */
    TerminalSetCursor32((prompt_start_col + CommandPos) * scr->font_width, cur_y);
}

static void ShellReplaceCommand(const char *Replacement)
{
    strcpy(Command, Replacement);
    CommandLength = (uint32_t)strlen(Command);
    CommandPos = CommandLength;
    RedrawCommandPrompt();
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
    CommandPos = 0;
    HistoryCount = 0;
    HistoryPosition = 0;
    HistoryBrowsing = false;
    Command[0] = '\0';
    WritePrompt();
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
                CommandPos = 0;
                Command[0] = '\0';
                WritePrompt();
            }
            continue;
        }

        /* History Navigation */
        if (Key == KBD_KEY_UP)   { ShellHistoryUp(); continue; }
        if (Key == KBD_KEY_DOWN) { ShellHistoryDown(); continue; }

        /* Left / Right Arrow Cursor Movement */
        if (Key == KBD_KEY_LEFT)
        {
            if (CommandPos > 0)
            {
                CommandPos--;
                CursorMoveLeft(TerminalGetScreen(), 1);
            }
            continue;
        }

        if (Key == KBD_KEY_RIGHT)
        {
            if (CommandPos < CommandLength)
            {
                CommandPos++;
                CursorMoveRight(TerminalGetScreen(), 1);
            }
            continue;
        }

        if (Key == KBD_KEY_HOME)
        {
            CommandPos = 0;
            RedrawCommandPrompt();
            continue;
        }

        if (Key == KBD_KEY_END)
        {
            CommandPos = CommandLength;
            RedrawCommandPrompt();
            continue;
        }

        if (Key == KBD_KEY_DELETE)
        {
            if (CommandPos < CommandLength)
            {
                for (uint32_t i = CommandPos; i < CommandLength; i++)
                {
                    Command[i] = Command[i + 1];
                }
                CommandLength--;
                RedrawCommandPrompt();
            }
            continue;
        }

        /* Ignore all other extended non-printable keys (0x80-0xFF) */
        if (Key >= 0x80U)
        {
            continue;
        }

        /* Enter Key */
        if (Character == '\n')
        {
            TerminalPutChar32('\n', PromColor);
            ShellSaveHistory();
            Command[CommandLength] = '\0';

            PARSED_COMMAND Parsed;
            ParserParse(Command, &Parsed);
            DispatcherDispatch(&Parsed);

            CommandLength = 0;
            CommandPos = 0;
            Command[0] = '\0';
            WritePrompt();
            continue;
        }

        /* Backspace Key */
        if (Character == '\b')
        {
            if (CommandPos > 0)
            {
                for (uint32_t i = CommandPos - 1; i < CommandLength; i++)
                {
                    Command[i] = Command[i + 1];
                }
                CommandLength--;
                CommandPos--;
                RedrawCommandPrompt();
            }
            continue;
        }

        /* Printable Characters */
        if (Character >= ' ' && Character <= '~')
        {
            if (CommandLength >= SHELL_MAX_LENGTH - 1U)
            {
                continue;
            }

            for (int32_t i = (int32_t)CommandLength; i >= (int32_t)CommandPos; i--)
            {
                Command[i + 1] = Command[i];
            }
            Command[CommandPos] = Character;
            CommandLength++;
            CommandPos++;
            Command[CommandLength] = '\0';
            RedrawCommandPrompt();
        }
    }
}
