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

#define SHELL_MAX_SHELLS      2U
#define SHELL_CMD_INIT_CAP   128U
#define SHELL_HISTORY_SIZE   16U
#define SHELL_CWD_CAPACITY   128U

typedef struct
{
    char *ActiveContext->Command;
    uint32_t ActiveContext->CommandCapacity;
    uint32_t ActiveContext->CommandLength;
    uint32_t ActiveContext->CommandPos;
    char *ActiveContext->History[SHELL_HISTORY_SIZE];
    char *ActiveContext->HistoryDraft;
    uint32_t ActiveContext->HistoryCount;
    uint32_t ActiveContext->HistoryPosition;
    bool ActiveContext->HistoryBrowsing;
    char Cwd[SHELL_CWD_CAPACITY];
    bool Exists;
} SHELL_CONTEXT;

static SHELL_CONTEXT Shells[SHELL_MAX_SHELLS];
static uint32_t ActiveShell = 0;
#define ActiveContext (&Shells[ActiveShell])


/* Grows the live command buffer so it can hold at least Need bytes
   (including the NUL terminator).  Returns false on allocation failure. */
static bool ShellCmdEnsure(uint32_t Need)
{
    if (Need <= ActiveContext->CommandCapacity) return true;
    uint32_t NewCap = ActiveContext->CommandCapacity ? ActiveContext->CommandCapacity : SHELL_CMD_INIT_CAP;
    while (NewCap < Need) NewCap *= 2U;
    char *New = (char *)kmalloc((uint64_t)NewCap);
    if (New == NULL) return false;
    if (ActiveContext->Command != NULL)
    {
        for (uint32_t i = 0; i <= ActiveContext->CommandLength; i++) New[i] = ActiveContext->Command[i];
        kfree(ActiveContext->Command);
    }
    else
    {
        New[0] = '\0';
    }
    ActiveContext->Command = New;
    ActiveContext->CommandCapacity = NewCap;
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
    TerminalWrite32(ActiveContext->Command, PromptColor);

    /* Fill remaining line length with spaces to clean up deleted chars */
    for (uint32_t i = PromptTextLength + ActiveContext->CommandLength; i < scr->cols; i++) {
        TerminalPutChar32(' ', PromptColor);
    }

    /* Position cursor at active editing offset ActiveContext->CommandPos, then show it */
    TerminalSetCursor32((PromptTextLength + ActiveContext->CommandPos) * scr->font_width, cur_y);
    CursorSetVisible(scr, true);
}

static void ShellReplaceCommand(const char *Replacement)
{
    uint32_t Len = (uint32_t)strlen(Replacement);
    if (!ShellCmdEnsure(Len + 1U)) return;
    strcpy(ActiveContext->Command, Replacement);
    ActiveContext->CommandLength = Len;
    ActiveContext->CommandPos = Len;
    RedrawCommandPrompt();
}

static void ShellSaveHistory(void)
{
    if (ActiveContext->CommandLength == 0) return;
    char *Copy = ShellCmdDup(ActiveContext->Command);
    if (Copy == NULL) return;
    if (ActiveContext->HistoryCount < SHELL_HISTORY_SIZE)
    {
        ActiveContext->History[ActiveContext->HistoryCount++] = Copy;
    }
    else
    {
        kfree(ActiveContext->History[0]);
        for (uint32_t Index = 1; Index < SHELL_HISTORY_SIZE; Index++)
            ActiveContext->History[Index - 1U] = ActiveContext->History[Index];
        ActiveContext->History[SHELL_HISTORY_SIZE - 1U] = Copy;
    }
    ActiveContext->HistoryPosition = ActiveContext->HistoryCount;
    ActiveContext->HistoryBrowsing = false;
}

static void ShellHistoryUp(void)
{
    if (ActiveContext->HistoryCount == 0) return;
    if (!ActiveContext->HistoryBrowsing)
    {
        if (ActiveContext->HistoryDraft != NULL) kfree(ActiveContext->HistoryDraft);
        ActiveContext->HistoryDraft = ShellCmdDup(ActiveContext->Command);
        ActiveContext->HistoryPosition = ActiveContext->HistoryCount;
        ActiveContext->HistoryBrowsing = true;
    }
    if (ActiveContext->HistoryPosition > 0)
        ShellReplaceCommand(ActiveContext->History[--ActiveContext->HistoryPosition]);
}

static void ShellHistoryDown(void)
{
    if (!ActiveContext->HistoryBrowsing) return;
    if (ActiveContext->HistoryPosition + 1U < ActiveContext->HistoryCount)
    {
        ShellReplaceCommand(ActiveContext->History[++ActiveContext->HistoryPosition]);
        return;
    }
    ActiveContext->HistoryBrowsing = false;
    ActiveContext->HistoryPosition = ActiveContext->HistoryCount;
    ShellReplaceCommand(ActiveContext->HistoryDraft != NULL ? ActiveContext->HistoryDraft : "");
}

static void ShellSaveCwd(void)
{
    if (Shells[ActiveShell].Exists)
        DiskFsPwd(Shells[ActiveShell].Cwd, SHELL_CWD_CAPACITY);
}

static void ShellRestoreCwd(uint32_t Index)
{
    if (Shells[Index].Exists)
        DiskFsChdirPath(Shells[Index].Cwd);
}

static void ShellFreeContext(uint32_t Index)
{
    SHELL_CONTEXT *Ctx = &Shells[Index];
    if (Ctx->Command != NULL) kfree(Ctx->Command);
    for (uint32_t i = 0; i < SHELL_HISTORY_SIZE; i++)
        if (Ctx->History[i] != NULL) kfree(Ctx->History[i]);
    if (Ctx->HistoryDraft != NULL) kfree(Ctx->HistoryDraft);
    Ctx->Command = NULL;
    Ctx->HistoryDraft = NULL;
    Ctx->CommandCapacity = Ctx->CommandLength = Ctx->CommandPos = 0;
    Ctx->HistoryCount = Ctx->HistoryPosition = 0;
    Ctx->HistoryBrowsing = false;
    Ctx->Cwd[0] = '\0';
    Ctx->Exists = false;
    for (uint32_t i = 0; i < SHELL_HISTORY_SIZE; i++) Ctx->History[i] = NULL;
}

static void ShellInitContext(uint32_t Index, const char *Cwd)
{
    uint32_t Saved = ActiveShell;
    SHELL_CONTEXT *Ctx = &Shells[Index];
    Ctx->Command = NULL;
    Ctx->CommandCapacity = Ctx->CommandLength = Ctx->CommandPos = 0;
    Ctx->HistoryDraft = NULL;
    Ctx->HistoryCount = Ctx->HistoryPosition = 0;
    Ctx->HistoryBrowsing = false;
    for (uint32_t i = 0; i < SHELL_HISTORY_SIZE; i++) Ctx->History[i] = NULL;
    strncpy(Ctx->Cwd, Cwd, SHELL_CWD_CAPACITY - 1U);
    Ctx->Cwd[SHELL_CWD_CAPACITY - 1U] = '\0';
    Ctx->Exists = true;
    ActiveShell = Index;
    ShellCmdEnsure(SHELL_CMD_INIT_CAP);
    if (ActiveContext->Command != NULL) ActiveContext->Command[0] = '\0';
    ActiveShell = Saved;
}

static void ShellSwitchTo(uint32_t Index)
{
    if (Index >= SHELL_MAX_SHELLS || !Shells[Index].Exists || Index == ActiveShell || EditorIsActive())
        return;
    ShellSaveCwd();
    ActiveShell = Index;
    ShellRestoreCwd(Index);
    TerminalSetActivePane(Index);
    RedrawCommandPrompt();
}

static void ShellCreateSplit(void)
{
    if (Shells[1].Exists || EditorIsActive())
        return;
    ShellSaveCwd();
    ShellInitContext(1, Shells[0].Cwd);
    TerminalSplitEnable();

    uint32_t Old = ActiveShell;
    ActiveShell = 1;
    ShellRestoreCwd(1);
    TerminalSetActivePane(1);
    TerminalClear32(0x00000000U);
    WritePrompt();

    ActiveShell = Old;
    ShellRestoreCwd(Old);
    TerminalSetActivePane(Old);
    RedrawCommandPrompt();
}

static void ShellDeleteCurrent(void)
{
    if (!Shells[1].Exists || EditorIsActive())
        return;

    uint32_t Dead = ActiveShell;
    uint32_t Other = Dead ^ 1U;
    ShellSaveCwd();
    TerminalSplitDisablePromote(Other);

    SHELL_CONTEXT SavedOther = Shells[Other];
    ShellFreeContext(Dead);
    Shells[0] = SavedOther;
    Shells[1].Command = NULL;
    Shells[1].CommandCapacity = Shells[1].CommandLength = Shells[1].CommandPos = 0;
    Shells[1].HistoryDraft = NULL;
    Shells[1].HistoryCount = Shells[1].HistoryPosition = 0;
    Shells[1].HistoryBrowsing = false;
    Shells[1].Cwd[0] = '\0';
    Shells[1].Exists = false;
    for (uint32_t i = 0; i < SHELL_HISTORY_SIZE; i++) Shells[1].History[i] = NULL;

    ActiveShell = 0;
    ShellRestoreCwd(0);
    TerminalSetActivePane(0);
    RedrawCommandPrompt();
}

static void ShellHandleFunctionKey(uint8_t Key)
{
    if (Key == KBD_KEY_F2) ShellCreateSplit();
    else if (Key == KBD_KEY_F3 && Shells[1].Exists) ShellSwitchTo(ActiveShell ^ 1U);
    else if (Key == KBD_KEY_F1) ShellDeleteCurrent();
}

void ShellInitialize(void)
{
    ShellLoadPromptConfig();
    for (uint32_t i = 0; i < SHELL_MAX_SHELLS; i++)
    {
        Shells[i].Command = NULL;
        Shells[i].CommandCapacity = Shells[i].CommandLength = Shells[i].CommandPos = 0;
        Shells[i].HistoryDraft = NULL;
        Shells[i].HistoryCount = Shells[i].HistoryPosition = 0;
        Shells[i].HistoryBrowsing = false;
        Shells[i].Cwd[0] = '\0';
        Shells[i].Exists = false;
        for (uint32_t j = 0; j < SHELL_HISTORY_SIZE; j++) Shells[i].History[j] = NULL;
    }
    ActiveShell = 0;
    Shells[0].Exists = true;
    DiskFsPwd(Shells[0].Cwd, SHELL_CWD_CAPACITY);
    ShellCmdEnsure(SHELL_CMD_INIT_CAP);
    ActiveContext->CommandLength = 0;
    ActiveContext->CommandPos = 0;
    if (ActiveContext->Command != NULL) ActiveContext->Command[0] = '\0';
    WritePrompt();
}

void ShellUpdate(void)
{
    while (KbdAvailable())
    {
        uint8_t Key = KbdReadKey();
        char Character = (char)Key;

        if (Key == KBD_KEY_F1 || Key == KBD_KEY_F2 || Key == KBD_KEY_F3)
        {
            ShellHandleFunctionKey(Key);
            continue;
        }

        if (EditorIsActive())
        {
            EditorHandleKey(Key, KbdGetState().Ctrl, KbdGetState().Shift);
            if (!EditorIsActive())
            {
                ActiveContext->CommandLength = 0;
                ActiveContext->CommandPos = 0;
                ActiveContext->Command[0] = '\0';
                WritePrompt();
            }
            continue;
        }

        /* ActiveContext->History Navigation */
        if (Key == KBD_KEY_UP)   { ShellHistoryUp(); continue; }
        if (Key == KBD_KEY_DOWN) { ShellHistoryDown(); continue; }

        /* Left / Right Arrow Cursor Movement */
        if (Key == KBD_KEY_LEFT)
        {
            if (ActiveContext->CommandPos > 0)
            {
                ActiveContext->CommandPos--;
                CursorMoveLeft(TerminalGetScreen(), 1);
            }
            continue;
        }

        if (Key == KBD_KEY_RIGHT)
        {
            if (ActiveContext->CommandPos < ActiveContext->CommandLength)
            {
                ActiveContext->CommandPos++;
                CursorMoveRight(TerminalGetScreen(), 1);
            }
            continue;
        }

        if (Key == KBD_KEY_HOME)
        {
            ActiveContext->CommandPos = 0;
            RedrawCommandPrompt();
            continue;
        }

        if (Key == KBD_KEY_END)
        {
            ActiveContext->CommandPos = ActiveContext->CommandLength;
            RedrawCommandPrompt();
            continue;
        }

        if (Key == KBD_KEY_DELETE)
        {
            if (ActiveContext->CommandPos < ActiveContext->CommandLength)
            {
                for (uint32_t i = ActiveContext->CommandPos; i < ActiveContext->CommandLength; i++)
                {
                    ActiveContext->Command[i] = ActiveContext->Command[i + 1];
                }
                ActiveContext->CommandLength--;
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
            ActiveContext->Command[ActiveContext->CommandLength] = '\0';

            PARSED_COMMAND Parsed;
            ParserParse(ActiveContext->Command, &Parsed);
            DispatcherDispatch(&Parsed);
            ParserFree(&Parsed);
            ShellSaveCwd();

            ActiveContext->CommandLength = 0;
            ActiveContext->CommandPos = 0;
            ActiveContext->Command[0] = '\0';
            if (!EditorIsActive())
            {
                WritePrompt();
            }
            continue;
        }

        /* Backspace Key */
        if (Character == '\b')
        {
            if (ActiveContext->CommandPos > 0)
            {
                for (uint32_t i = ActiveContext->CommandPos - 1; i < ActiveContext->CommandLength; i++)
                {
                    ActiveContext->Command[i] = ActiveContext->Command[i + 1];
                }
                ActiveContext->CommandLength--;
                ActiveContext->CommandPos--;
                RedrawCommandPrompt();
            }
            continue;
        }

        /* Printable Characters */
        if (Character >= ' ' && Character <= '~')
        {
            if (!ShellCmdEnsure(ActiveContext->CommandLength + 2U))
            {
                continue;
            }

            for (int32_t i = (int32_t)ActiveContext->CommandLength; i >= (int32_t)ActiveContext->CommandPos; i--)
            {
                ActiveContext->Command[i + 1] = ActiveContext->Command[i];
            }
            ActiveContext->Command[ActiveContext->CommandPos] = Character;
            ActiveContext->CommandLength++;
            ActiveContext->CommandPos++;
            ActiveContext->Command[ActiveContext->CommandLength] = '\0';
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
