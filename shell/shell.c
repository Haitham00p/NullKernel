#include "shell.h"
#include "diskfs.h"
#include "drivers/input/keyboard/keyboard.h"
#include "kernel/terminal/terminal.h"
#include "include/cursor.h"
#include <stddef.h>
#include "mm/heap/heap.h"
#include "lib/string/string.h"
#include "dispatcher.h"
#include "parser.h"
#include "editor/editor.h"
#include "kernel/debug/debug.h"

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;
uint32_t PromColor = 0xFFFFFFFF;
uint32_t BackSpaceColor = 0x00000000;

/* Output text color for command output (printf/puts/cat), driven by /REG/color/op_fg.v */
uint32_t OutColor = 0x00808080U;
/* Prompt color, driven by /REG/color/p_fg.v (0xFFFFFFFF = default white) */
uint32_t PromptColor = 0xFFFFFFFFU;

static char *Command = NULL;
static uint32_t CommandCapacity = 0;
static uint32_t CommandLength = 0;
static uint32_t CommandPos = 0;

#define SHELL_CMD_INIT_CAP   128U
#define SHELL_HISTORY_SIZE   16U
static char *History[SHELL_HISTORY_SIZE];
static char *HistoryDraft = NULL;

/* Grows the live command buffer so it can hold at least Need bytes
   (including the NUL terminator).  Returns false on allocation failure. */
static bool ShellCmdEnsure(uint32_t Need)
{
    if (Need <= CommandCapacity) return true;
    uint32_t NewCap = CommandCapacity ? CommandCapacity : SHELL_CMD_INIT_CAP;
    while (NewCap < Need) NewCap *= 2U;
    char *New = (char *)kmalloc((uint64_t)NewCap);
    if (New == NULL) return false;
    if (Command != NULL)
    {
        for (uint32_t i = 0; i <= CommandLength; i++) New[i] = Command[i];
        kfree(Command);
    }
    else
    {
        New[0] = '\0';
    }
    Command = New;
    CommandCapacity = NewCap;
    return true;
}

/* Heap-copies Src; returns NULL on allocation failure. */
static char *ShellCmdDup(const char *Src)
{
    uint32_t Len = (uint32_t)strlen(Src);
    char *Out = (char *)kmalloc((uint64_t)Len + 1U);
    if (Out == NULL) return NULL;
    for (uint32_t i = 0; i <= Len; i++) Out[i] = Src[i];
    return Out;
}
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

static char PromptText[256];
static uint32_t PromptTextLength = 0;

/* Registry-driven prompt components. Defaults match the historic look until
   /REG/SYSTEM/HOSTNAME.V and /REG/SHELL/PROMPT.V override them at boot. */
static char g_HostName[64] = "NullOS";
static char g_PromptSuffix[16] = "> ";

static uint32_t PromptAppend(char *Buf, uint32_t Max, uint32_t At,
                             const char *Str)
{
    while (*Str != '\0' && At + 1U < Max)
    {
        Buf[At++] = *Str++;
    }
    return At;
}

static void ShellLoadPromptConfig(void)
{
    uint8_t Buf[64];
    uint32_t N = 0;

    if (!DiskFsIsMounted())
    {
        return;
    }
    if (DiskFsReadInPath("/REG/SYSTEM", "HOSTNAME.V", Buf, sizeof(Buf) - 1U, &N) &&
        N > 0)
    {
        while (N > 0U && (Buf[N - 1U] == '\n' || Buf[N - 1U] == '\r'))
        {
            N--;
        }
        if (N > 0U && N < sizeof(g_HostName))
        {
            memcpy(g_HostName, Buf, N);
            g_HostName[N] = '\0';
        }
    }
    if (DiskFsReadInPath("/REG/SHELL", "PROMPT.V", Buf, sizeof(Buf) - 1U, &N) &&
        N > 0)
    {
        while (N > 0U && (Buf[N - 1U] == '\n' || Buf[N - 1U] == '\r'))
        {
            N--;
        }
        if (N > 0U && N < sizeof(g_PromptSuffix))
        {
            memcpy(g_PromptSuffix, Buf, N);
            g_PromptSuffix[N] = '\0';
        }
    }
}

static void ComposePrompt(void)
{
    char Path[128];
    uint32_t Len = 0;

    DiskFsPwd(Path, sizeof(Path));
    Len = PromptAppend(PromptText, sizeof(PromptText), Len, "Developer@");
    Len = PromptAppend(PromptText, sizeof(PromptText), Len, g_HostName);
    Len = PromptAppend(PromptText, sizeof(PromptText), Len, ":");
    Len = PromptAppend(PromptText, sizeof(PromptText), Len, Path);
    Len = PromptAppend(PromptText, sizeof(PromptText), Len, g_PromptSuffix);
    PromptText[Len] = '\0';
    PromptTextLength = Len;
}

static void WritePrompt(void) {
    uint32_t prompt_color = ComputePromptColor(PromptColor);
    ComposePrompt();
    TerminalWrite32(PromptText, prompt_color);
}

static void RedrawCommandPrompt(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t cur_x = 0, cur_y = 0;
    TerminalGetCursor32(&cur_x, &cur_y);

    /* Hide the cursor while we rewrite the whole line so the inverted cursor
       block does not sweep across the prompt on every keystroke. */
    CursorSetVisible(scr, false);

    /* Clear current prompt line content */
    TerminalSetCursor32(0, cur_y);
    WritePrompt();
    TerminalWrite32(Command, PromptColor);

    /* Fill remaining line length with spaces to clean up deleted chars */
    for (uint32_t i = PromptTextLength + CommandLength; i < scr->cols; i++) {
        TerminalPutChar32(' ', PromptColor);
    }

    /* Position cursor at active editing offset CommandPos, then show it */
    TerminalSetCursor32((PromptTextLength + CommandPos) * scr->font_width, cur_y);
    CursorSetVisible(scr, true);
}

static void ShellReplaceCommand(const char *Replacement)
{
    uint32_t Len = (uint32_t)strlen(Replacement);
    if (!ShellCmdEnsure(Len + 1U)) return;
    strcpy(Command, Replacement);
    CommandLength = Len;
    CommandPos = Len;
    RedrawCommandPrompt();
}

static void ShellSaveHistory(void)
{
    if (CommandLength == 0) return;
    char *Copy = ShellCmdDup(Command);
    if (Copy == NULL) return;
    if (HistoryCount < SHELL_HISTORY_SIZE)
    {
        History[HistoryCount++] = Copy;
    }
    else
    {
        kfree(History[0]);
        for (uint32_t Index = 1; Index < SHELL_HISTORY_SIZE; Index++)
            History[Index - 1U] = History[Index];
        History[SHELL_HISTORY_SIZE - 1U] = Copy;
    }
    HistoryPosition = HistoryCount;
    HistoryBrowsing = false;
}

static void ShellHistoryUp(void)
{
    if (HistoryCount == 0) return;
    if (!HistoryBrowsing)
    {
        if (HistoryDraft != NULL) kfree(HistoryDraft);
        HistoryDraft = ShellCmdDup(Command);
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
    ShellReplaceCommand(HistoryDraft != NULL ? HistoryDraft : "");
}

void ShellInitialize(void)
{
    ShellLoadPromptConfig();
    ShellCmdEnsure(SHELL_CMD_INIT_CAP);
    CommandLength = 0;
    CommandPos = 0;
    HistoryCount = 0;
    HistoryPosition = 0;
    HistoryBrowsing = false;
    if (Command != NULL) Command[0] = '\0';
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
            EditorHandleKey(Key, KbdGetState().Ctrl, KbdGetState().Shift);
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
            ParserFree(&Parsed);

            CommandLength = 0;
            CommandPos = 0;
            Command[0] = '\0';
            if (!EditorIsActive())
            {
                WritePrompt();
            }
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
            if (!ShellCmdEnsure(CommandLength + 2U))
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

void DbgBootLogFlush(void)
{
    DBG_CATEGORY Cat;

    if (!DiskFsIsMounted())
    {
        return;
    }
    DiskFsMkdirAll("/jor");
    for (Cat = 0; (uint32_t)Cat < (uint32_t)DBG_CAT_COUNT; Cat++)
    {
        uint32_t Len = DbgCatLength(Cat);
        if (Len > 0)
        {
            DiskFsWriteInPath("/jor", DbgCatName(Cat), DbgCatBuffer(Cat), Len);
        }
    }
}
