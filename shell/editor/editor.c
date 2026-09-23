#include "editor.h"

#include "libc/stdlib.h"
#include "include/diskfs.h"
#include "kernel/terminal/terminal.h"
#include "include/cursor.h"
#include "drivers/input/keyboard/keyboard.h"
#include "drivers/video_framebuffer/framebuffer.h"
#include "lib/string/string.h"

#define EDITOR_CAPACITY 4096U
#define EDITOR_PATH_CAPACITY 32U
#define EDITOR_STATUS_CELLS 16U

#define EDITOR_BG 0x00000000U
#define EDITOR_FG 0xFFFFFFFFU

#define EDITOR_C_COMMENT    0xFF8A8A8AU /* grey   */
#define EDITOR_C_STRING     0xFF22C55EU /* green  */
#define EDITOR_C_NUMBER     0xFFFFB86CU /* orange */
#define EDITOR_C_HEADER     0xFFC586C0U /* purple (includes) */
#define EDITOR_C_DIRECTIVE  0xFFC586C0U /* purple (asm) */
#define EDITOR_C_KEYWORD    0xFF4FC1FFU /* blue   */
#define EDITOR_C_TYPE       0xFF4EC9B0U /* teal   */
#define EDITOR_C_API        0xFFFFD866U /* yellow */
#define EDITOR_C_LABEL      0xFFFFFFFFU /* white  */
#define EDITOR_GRAY         0x00808080U
#define EDITOR_V_COLOR      0x00FF8000U

static bool EditorIsAlpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool EditorIsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool EditorIsIdent(char c)
{
    return EditorIsAlpha(c) || EditorIsDigit(c);
}

static void EditorDeleteAt(uint32_t Index);

static char EditorPath[EDITOR_PATH_CAPACITY];
static char EditorBuffer[EDITOR_CAPACITY];
static uint32_t EditorLength;
static uint32_t EditorPos;
static uint32_t EditorTopLine;
static bool EditorRunning;
static bool EditorDirty;
static bool EditorIsCFile;
static bool EditorIsVFile;
static bool EditorIsAsmFile;
static char EditorDir[64];
static char EditorName[64];
static bool EditorHasDir;

/* Selection / clipboard state for shift-arrow word selection, Ctrl+Shift+C
   (copy) and Ctrl+Shift+V (paste).  A selection runs from EditorSelAnchor
   to EditorPos; when EditorSelecting is false there is no selection. */
static uint32_t EditorSelAnchor;
static bool     EditorSelecting;
static char     EditorClipboard[EDITOR_CAPACITY];
static uint32_t EditorClipLen;

static uint32_t EditorSelMin(void)
{
    return EditorSelecting ? (EditorSelAnchor < EditorPos ? EditorSelAnchor : EditorPos)
                           : EditorPos;
}
static uint32_t EditorSelMax(void)
{
    return EditorSelecting ? (EditorSelAnchor > EditorPos ? EditorSelAnchor : EditorPos)
                           : EditorPos;
}
static void EditorClearSelection(void)
{
    EditorSelecting = false;
}
static uint32_t EditorPrevWord(uint32_t Pos)
{
    while (Pos > 0U && !EditorIsIdent(EditorBuffer[Pos - 1U])) Pos--;
    while (Pos > 0U && EditorIsIdent(EditorBuffer[Pos - 1U])) Pos--;
    return Pos;
}
static uint32_t EditorNextWord(uint32_t Pos)
{
    while (Pos < EditorLength && !EditorIsIdent(EditorBuffer[Pos])) Pos++;
    while (Pos < EditorLength && EditorIsIdent(EditorBuffer[Pos])) Pos++;
    return Pos;
}

static uint32_t EditorLineStart(uint32_t Pos)
{
    while (Pos > 0U && EditorBuffer[Pos - 1U] != '\n')
    {
        Pos--;
    }
    return Pos;
}

static uint32_t EditorLineEnd(uint32_t Pos)
{
    while (Pos < EditorLength && EditorBuffer[Pos] != '\n')
    {
        Pos++;
    }
    return Pos;
}

static uint32_t EditorLineStartByLine(uint32_t Line)
{
    uint32_t Idx = 0;
    uint32_t L = 0;

    while (L < Line && Idx < EditorLength)
    {
        if (EditorBuffer[Idx++] == '\n')
        {
            L++;
        }
    }
    return Idx;
}

static void EditorLineCol(uint32_t Pos, uint32_t *Line, uint32_t *Col)
{
    uint32_t L = 0;
    uint32_t C = 0;

    for (uint32_t i = 0; i < Pos; i++)
    {
        if (EditorBuffer[i] == '\n')
        {
            L++;
            C = 0;
        }
        else
        {
            C++;
        }
    }
    if (Line) *Line = L;
    if (Col) *Col = C;
}

static uint32_t EditorAppendNum(char *St, uint32_t n, uint32_t V)
{
    char Tmp[12];
    uint32_t k = 0;

    if (V == 0U)
    {
        St[n++] = '0';
        return n;
    }
    while (V != 0U && k < 11U)
    {
        Tmp[k++] = (char)('0' + (V % 10U));
        V /= 10U;
    }
    while (k > 0U)
    {
        St[n++] = Tmp[--k];
    }
    return n;
}

static void EditorDrawStatus(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t Line = 0;
    uint32_t Col = 0;
    char St[24];
    uint32_t n = 0;
    uint32_t Start;
    uint32_t i;

    EditorLineCol(EditorPos, &Line, &Col);
    St[n++] = 'L';
    St[n++] = 'n';
    St[n++] = ' ';
    n = EditorAppendNum(St, n, Line + 1U);
    St[n++] = ' ';
    St[n++] = 'C';
    St[n++] = 'o';
    St[n++] = 'l';
    St[n++] = ' ';
    n = EditorAppendNum(St, n, Col + 1U);
    St[n] = '\0';

    TerminalSetCursor32((scr->cols - EDITOR_STATUS_CELLS) * scr->font_width,
                        (scr->rows - 1U) * scr->font_height);
    for (i = 0; i < EDITOR_STATUS_CELLS; i++)
    {
        TerminalPutChar32(' ', EDITOR_BG);
    }
    Start = n < EDITOR_STATUS_CELLS ? scr->cols - n : scr->cols - EDITOR_STATUS_CELLS;
    TerminalSetCursor32(Start * scr->font_width,
                        (scr->rows - 1U) * scr->font_height);
    TerminalWrite32(St, EDITOR_FG);
}

static void EditorCursorTo(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t Line = 0;
    uint32_t Col = 0;
    uint32_t Row;

    EditorLineCol(EditorPos, &Line, &Col);
    Row = Line - EditorTopLine;
    if (Row > scr->rows - 2U)
    {
        Row = scr->rows - 2U;
    }
    if (Col > scr->cols - 1U)
    {
        Col = scr->cols - 1U;
    }
    TerminalSetCursor32(Col * scr->font_width, Row * scr->font_height);
}

static bool EditorClampScroll(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t Line = 0;
    uint32_t Col = 0;
    uint32_t textRows = scr->rows >= 2U ? scr->rows - 1U : 1U;

    EditorLineCol(EditorPos, &Line, &Col);
    if (Line < EditorTopLine)
    {
        EditorTopLine = Line;
        return true;
    }
    if (Line >= EditorTopLine + textRows)
    {
        EditorTopLine = Line - textRows + 1U;
        return true;
    }
    return false;
}

static void EditorPaintLineAsm(uint32_t Line)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t Start = EditorLineStartByLine(Line);
    uint32_t End = EditorLineEnd(Start);
    uint32_t Row = Line - EditorTopLine;
    uint32_t i = Start;
    uint32_t cells = 0;
    char Ch;

    if (Row > scr->rows - 2U)
    {
        return;
    }
    TerminalSetCursor32(0, Row * scr->font_height);

    while (i < End && cells < scr->cols)
    {
        Ch = EditorBuffer[i];

        if (Ch == ' ' || Ch == '\t' || Ch == '\r')
        {
            TerminalPutChar32(Ch, EDITOR_FG);
            i++;
            cells++;
            continue;
        }
        if (Ch == ';')
        {
            while (i < End && cells < scr->cols)
            {
                TerminalPutChar32(EditorBuffer[i], EDITOR_C_COMMENT);
                i++;
                cells++;
            }
            break;
        }
        if (Ch == '"' || Ch == '\'')
        {
            char end = Ch;
            TerminalPutChar32(Ch, EDITOR_C_STRING);
            i++;
            cells++;
            while (i < End && cells < scr->cols)
            {
                char s = EditorBuffer[i];
                if (s == end)
                {
                    TerminalPutChar32(end, EDITOR_C_STRING);
                    i++;
                    cells++;
                    break;
                }
                if (s == '\\' && i + 1U < End)
                {
                    TerminalPutChar32('\\', EDITOR_C_STRING);
                    TerminalPutChar32(EditorBuffer[i + 1U], EDITOR_C_STRING);
                    i += 2U;
                    cells += 2U;
                    continue;
                }
                TerminalPutChar32(s, EDITOR_C_STRING);
                i++;
                cells++;
            }
            continue;
        }
        if (((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') ||
             Ch == '_' || Ch == '.') ||
            (Ch == '%'))
        {
            uint32_t j = i;
            while (j < End && ((EditorBuffer[j] >= 'a' && EditorBuffer[j] <= 'z') ||
                   (EditorBuffer[j] >= 'A' && EditorBuffer[j] <= 'Z') ||
                   (EditorBuffer[j] >= '0' && EditorBuffer[j] <= '9') ||
                   EditorBuffer[j] == '_' || EditorBuffer[j] == '.' ||
                   EditorBuffer[j] == '%'))
            {
                j++;
            }
            uint32_t wlen = j - i;
            uint32_t color = EDITOR_FG;
            if (HlIsAsmMnemonic(EditorBuffer + i, wlen))
            {
                color = EDITOR_C_KEYWORD;
            }
            else if (HlIsAsmDirective(EditorBuffer + i, wlen))
            {
                color = EDITOR_C_DIRECTIVE;
            }
            else if (HlIsAsmRegister(EditorBuffer + i, wlen))
            {
                color = EDITOR_C_NUMBER;
            }
            else if (EditorBuffer[i] == '%')
            {
                color = EDITOR_C_DIRECTIVE;
            }
            else if (EditorBuffer[i] == '.' || HlIsAsmApi(EditorBuffer + i, wlen))
            {
                color = EDITOR_C_API;
            }
            while (i < j && cells < scr->cols)
            {
                TerminalPutChar32(EditorBuffer[i], color);
                i++;
                cells++;
            }
            continue;
        }
        if ((Ch >= '0' && Ch <= '9') ||
            (Ch == '$' && i + 1U < End && EditorBuffer[i + 1U] >= '0' &&
             EditorBuffer[i + 1U] <= '9'))
        {
            uint32_t j = i;
            while (j < End && ((EditorBuffer[j] >= '0' && EditorBuffer[j] <= '9') ||
                   EditorBuffer[j] == 'x' || EditorBuffer[j] == 'X' ||
                   EditorBuffer[j] == '$' ||
                   (EditorBuffer[j] >= 'a' && EditorBuffer[j] <= 'f') ||
                   (EditorBuffer[j] >= 'A' && EditorBuffer[j] <= 'F')))
            {
                j++;
            }
            while (i < j && cells < scr->cols)
            {
                TerminalPutChar32(EditorBuffer[i], EDITOR_C_NUMBER);
                i++;
                cells++;
            }
            continue;
        }
        TerminalPutChar32(Ch, EDITOR_FG);
        i++;
        cells++;
    }

    for (uint32_t c = cells; c < scr->cols; c++)
    {
        TerminalPutChar32(' ', EDITOR_BG);
    }
}

static void EditorPaintLine(uint32_t Line, bool *inBlock)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t Start = EditorLineStartByLine(Line);
    uint32_t End = EditorLineEnd(Start);
    uint32_t Row = Line - EditorTopLine;
    uint32_t i = Start;
    uint32_t cells = 0;
    char Ch;

    if (Row > scr->rows - 2U)
    {
        return;
    }
    TerminalSetCursor32(0, Row * scr->font_height);

    if (EditorIsAsmFile)
    {
        EditorPaintLineAsm(Line);
        return;
    }

    if (!EditorIsCFile)
    {
        uint32_t col = EditorIsVFile ? EDITOR_V_COLOR : EDITOR_GRAY;
        while (i < End && cells < scr->cols)
        {
            TerminalPutChar32(EditorBuffer[i], col);
            i++;
            cells++;
        }
        for (uint32_t c = cells; c < scr->cols; c++)
        {
            TerminalPutChar32(' ', EDITOR_BG);
        }
        return;
    }

    while (i < End && cells < scr->cols)
    {
        Ch = EditorBuffer[i];

        if (*inBlock)
        {
            if (Ch == '*' && i + 1U < End && EditorBuffer[i + 1U] == '/')
            {
                TerminalPutChar32('*', EDITOR_C_COMMENT);
                TerminalPutChar32('/', EDITOR_C_COMMENT);
                i += 2U;
                cells += 2U;
                *inBlock = false;
            }
            else
            {
                TerminalPutChar32(Ch, EDITOR_C_COMMENT);
                i++;
                cells++;
            }
            continue;
        }

        if (Ch == '/')
        {
            if (i + 1U < End && EditorBuffer[i + 1U] == '/')
            {
                while (i < End && cells < scr->cols)
                {
                    TerminalPutChar32(EditorBuffer[i], EDITOR_C_COMMENT);
                    i++;
                    cells++;
                }
                break;
            }
            if (i + 1U < End && EditorBuffer[i + 1U] == '*')
            {
                TerminalPutChar32('/', EDITOR_C_COMMENT);
                TerminalPutChar32('*', EDITOR_C_COMMENT);
                i += 2U;
                cells += 2U;
                *inBlock = true;
                continue;
            }
            TerminalPutChar32('/', EDITOR_FG);
            i++;
            cells++;
            continue;
        }

        if (Ch == '<')
        {
            uint32_t j = i + 1U;
            bool IsHeader = false;
            while (j < End && EditorBuffer[j] != '>')
            {
                if (EditorBuffer[j] == '.')
                {
                    IsHeader = true;
                }
                j++;
            }
            uint32_t SpanEnd = (j < End) ? j + 1U : End;
            uint32_t color = IsHeader ? EDITOR_C_HEADER : EDITOR_FG;
            while (i < SpanEnd && cells < scr->cols)
            {
                TerminalPutChar32(EditorBuffer[i], color);
                i++;
                cells++;
            }
            continue;
        }

        if (Ch == '"')
        {
            TerminalPutChar32('"', EDITOR_C_STRING);
            i++;
            cells++;
            while (i < End && cells < scr->cols)
            {
                char s = EditorBuffer[i];
                if (s == '"')
                {
                    TerminalPutChar32('"', EDITOR_C_STRING);
                    i++;
                    cells++;
                    break;
                }
                if (s == '\\' && i + 1U < End)
                {
                    TerminalPutChar32('\\', EDITOR_C_STRING);
                    TerminalPutChar32(EditorBuffer[i + 1U], EDITOR_C_STRING);
                    i += 2U;
                    cells += 2U;
                    continue;
                }
                TerminalPutChar32(s, EDITOR_C_STRING);
                i++;
                cells++;
            }
            continue;
        }

        if (EditorIsDigit(Ch) || (Ch == '.' && i + 1U < End && EditorIsDigit(EditorBuffer[i + 1U])))
        {
            while (i < End && cells < scr->cols &&
                   (EditorIsDigit(EditorBuffer[i]) || EditorBuffer[i] == '.' ||
                    EditorBuffer[i] == 'x' || EditorBuffer[i] == 'X' ||
                    (EditorBuffer[i] >= 'a' && EditorBuffer[i] <= 'f') ||
                    (EditorBuffer[i] >= 'A' && EditorBuffer[i] <= 'F')))
            {
                TerminalPutChar32(EditorBuffer[i], EDITOR_C_NUMBER);
                i++;
                cells++;
            }
            continue;
        }

        if (EditorIsAlpha(Ch))
        {
            uint32_t j = i;
            bool HasUpper = false;
            while (j < End && EditorIsIdent(EditorBuffer[j]))
            {
                if (EditorBuffer[j] >= 'A' && EditorBuffer[j] <= 'Z')
                {
                    HasUpper = true;
                }
                j++;
            }
            uint32_t k = j;
            while (k < End && EditorBuffer[k] == ' ')
            {
                k++;
            }
            bool IsApi = HasUpper || (k < End && EditorBuffer[k] == '(');
            uint32_t color = EDITOR_FG;
            uint32_t wl = j - i;
            if (wl < 64U)
            {
                char wbuf[64];
                memcpy(wbuf, EditorBuffer + i, wl);
                wbuf[wl] = '\0';
                if (HlIsCKeyword(wbuf, wl))
                {
                    color = EDITOR_C_KEYWORD;
                }
                else if (HlIsCType(wbuf, wl))
                {
                    color = EDITOR_C_TYPE;
                }
                else if (IsApi)
                {
                    color = EDITOR_C_API;
                }
            }
            else if (IsApi)
            {
                color = EDITOR_C_API;
            }
            while (i < j && cells < scr->cols)
            {
                TerminalPutChar32(EditorBuffer[i], color);
                i++;
                cells++;
            }
            continue;
        }

        TerminalPutChar32(Ch, EDITOR_FG);
        i++;
        cells++;
    }

    for (uint32_t c = cells; c < scr->cols; c++)
    {
        TerminalPutChar32(' ', EDITOR_BG);
    }
}

static bool EditorBlockStateAtLine(uint32_t Line)
{
    uint32_t Start = EditorLineStartByLine(Line);
    uint32_t i = 0;
    bool inBlock = false;

    while (i < Start && i < EditorLength)
    {
        char c = EditorBuffer[i];
        if (inBlock)
        {
            if (c == '*' && i + 1U < EditorLength && EditorBuffer[i + 1U] == '/')
            {
                inBlock = false;
                i += 2U;
                continue;
            }
            i++;
            continue;
        }
        if (c == '/')
        {
            if (i + 1U < EditorLength && EditorBuffer[i + 1U] == '/')
            {
                while (i < EditorLength && EditorBuffer[i] != '\n')
                {
                    i++;
                }
                continue;
            }
            if (i + 1U < EditorLength && EditorBuffer[i + 1U] == '*')
            {
                inBlock = true;
                i += 2U;
                continue;
            }
        }
        i++;
    }
    return inBlock;
}

static void EditorPaintSelection(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t textRows = scr->rows >= 2U ? scr->rows - 1U : 1U;
    uint32_t a = EditorSelMin();
    uint32_t b = EditorSelMax();
    uint32_t i;

    if (a >= b)
    {
        return;
    }
    for (i = a; i < b; i++)
    {
        uint32_t Line = 0;
        uint32_t Col = 0;
        uint32_t Row;
        uint32_t X;
        uint32_t Y;

        EditorLineCol(i, &Line, &Col);
        if (Line < EditorTopLine || Line >= EditorTopLine + textRows)
        {
            continue;
        }
        Row = Line - EditorTopLine;
        X = Col * scr->font_width;
        Y = Row * scr->font_height;
        FbFillRect32(X, Y, scr->font_width, scr->font_height, 0x004060A0U);
        if (EditorBuffer[i] != '\n')
        {
            FbDrawChar32((uint8_t)EditorBuffer[i], X, Y, 0x00FFFFFFU);
        }
    }
}

static void EditorPaintText(void)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t textRows = scr->rows >= 2U ? scr->rows - 1U : 1U;
    bool inBlock = false;

    /* Hide the cursor while we repaint: TerminalPutChar32 moves and renders
       the cursor on every glyph, so a visible cursor would sweep across the
       screen (the "random cursor" glitch). We restore it once at the end. */
    CursorSetVisible(scr, false);
    for (uint32_t r = 0; r < textRows; r++)
    {
        EditorPaintLine(EditorTopLine + r, &inBlock);
    }
    EditorPaintSelection();
    EditorDrawStatus();
    EditorCursorTo();
    CursorSetVisible(scr, true);
    TerminalRender(scr);
}

static void EditorRepaintLine(uint32_t Line)
{
    term_screen_t *scr = TerminalGetScreen();
    uint32_t Row = Line - EditorTopLine;
    bool inBlock;

    if (Row > scr->rows - 2U)
    {
        EditorPaintText();
        return;
    }
    CursorSetVisible(scr, false);
    inBlock = EditorBlockStateAtLine(Line);
    EditorPaintLine(Line, &inBlock);
    EditorDrawStatus();
    EditorCursorTo();
    CursorSetVisible(scr, true);
    TerminalRender(scr);
}

static void EditorRefresh(void)
{
    term_screen_t *scr = TerminalGetScreen();
    CursorSetVisible(scr, false);
    EditorDrawStatus();
    EditorCursorTo();
    CursorSetVisible(scr, true);
    TerminalRender(scr);
}

static void EditorInsertChar(char Character)
{
    if (EditorLength >= EDITOR_CAPACITY - 1U)
    {
        return;
    }
    for (uint32_t i = EditorLength; i > EditorPos; i--)
    {
        EditorBuffer[i] = EditorBuffer[i - 1U];
    }
    EditorBuffer[EditorPos] = Character;
    EditorLength++;
    EditorPos++;
    EditorBuffer[EditorLength] = '\0';
    EditorDirty = true;
}

static void EditorDeleteAt(uint32_t Index)
{
    for (uint32_t i = Index; i + 1U < EditorLength; i++)
    {
        EditorBuffer[i] = EditorBuffer[i + 1U];
    }
    EditorLength--;
    EditorBuffer[EditorLength] = '\0';
    EditorDirty = true;
}

static bool EditorPathIsC(const char *Path)
{
    uint32_t len = 0;

    while (Path[len] != '\0')
    {
        len++;
    }
    if (len < 2U)
    {
        return false;
    }
    if (Path[len - 1U] != 'c' && Path[len - 1U] != 'C')
    {
        return false;
    }
    if (Path[len - 2U] != '.')
    {
        return false;
    }
    return true;
}

static bool EditorPathIsV(const char *Path)
{
    uint32_t len = 0;

    while (Path[len] != '\0')
    {
        len++;
    }
    if (len < 2U)
    {
        return false;
    }
    if (Path[len - 1U] != 'v' && Path[len - 1U] != 'V')
    {
        return false;
    }
    if (Path[len - 2U] != '.')
    {
        return false;
    }
    return true;
}

static bool EditorPathIsAsm(const char *Path)
{
    uint32_t len = 0;

    while (Path[len] != '\0')
    {
        len++;
    }
    if (len < 4U)
    {
        return false;
    }
    char e1 = Path[len - 1U];
    char e2 = Path[len - 2U];
    char e3 = Path[len - 3U];
    char dot = Path[len - 4U];
    if (dot != '.')
    {
        return false;
    }
    if (e3 == 'a' || e3 == 'A')
    {
        return (e2 == 's' || e2 == 'S') && (e1 == 'm' || e1 == 'M');
    }
    if (e3 == 's' || e3 == 'S')
    {
        return (e2 == 'm' || e2 == 'M') && (e1 == ' ' || e1 == '\0');
    }
    return false;
}

static bool EditorSplitPath(const char *Path, char *Dir, char *Name, bool *HasDir)
{
    uint32_t len = 0;
    uint32_t last = 0xFFFFFFFFU;
    uint32_t i;
    uint32_t d;

    while (Path[len] != '\0')
    {
        if (Path[len] == '/')
        {
            last = len;
        }
        len++;
    }
    if (len >= 64U)
    {
        return false;
    }
    if (last == 0xFFFFFFFFU)
    {
        *HasDir = false;
        for (i = 0; i < len; i++)
        {
            Name[i] = Path[i];
        }
        Name[len] = '\0';
        return true;
    }
    *HasDir = true;
    for (d = 0; d < last; d++)
    {
        Dir[d] = Path[d];
    }
    Dir[last] = '\0';
    for (i = 0; Path[last + 1U + i] != '\0'; i++)
    {
        Name[i] = Path[last + 1U + i];
    }
    Name[i] = '\0';
    return true;
}

bool EditorOpen(const char *Path)
{
    uint32_t Size = 0;
    uint32_t w = 0;

    if (Path == NULL || Path[0] == '\0' || strlen(Path) >= EDITOR_PATH_CAPACITY)
    {
        return false;
    }
    if (!DiskFsIsMounted() && !DiskFsMount())
    {
        return false;
    }

    strcpy(EditorPath, Path);
    EditorIsCFile = EditorPathIsC(Path);
    EditorIsVFile = EditorPathIsV(Path);
    EditorIsAsmFile = EditorPathIsAsm(Path);
    if (!EditorSplitPath(Path, EditorDir, EditorName, &EditorHasDir))
    {
        return false;
    }
    EditorLength = 0;
    EditorPos = 0;
    EditorTopLine = 0;
    EditorDirty = false;
    EditorBuffer[0] = '\0';

    if (EditorHasDir)
    {
        if (DiskFsReadInPath(EditorDir, EditorName, EditorBuffer, EDITOR_CAPACITY - 1U, &Size))
        {
            if (Size >= EDITOR_CAPACITY - 1U)
            {
                return false;
            }
            for (uint32_t i = 0; i < Size; i++)
            {
                char Ch = EditorBuffer[i];

                if (Ch == '\r')
                {
                    if (i + 1U < Size && EditorBuffer[i + 1U] == '\n')
                    {
                        continue;
                    }
                    Ch = '\n';
                }
                EditorBuffer[w++] = Ch;
            }
            EditorLength = w;
            EditorBuffer[EditorLength] = '\0';
        }
    }
    else if (DiskFsRead(EditorName, EditorBuffer, EDITOR_CAPACITY - 1U, &Size))
    {
        if (Size >= EDITOR_CAPACITY - 1U)
        {
            return false;
        }
        for (uint32_t i = 0; i < Size; i++)
        {
            char Ch = EditorBuffer[i];

            if (Ch == '\r')
            {
                if (i + 1U < Size && EditorBuffer[i + 1U] == '\n')
                {
                    continue;
                }
                Ch = '\n';
            }
            EditorBuffer[w++] = Ch;
        }
        EditorLength = w;
        EditorBuffer[EditorLength] = '\0';
    }
    else if (!DiskFsTouch(EditorName))
    {
        return false;
    }

    EditorRunning = true;
    EditorPaintText();
    return true;
}

bool EditorIsActive(void)
{
    return EditorRunning;
}

void EditorHandleKey(uint8_t Key, bool ControlHeld, bool ShiftHeld)
{
    if (!EditorRunning)
    {
        return;
    }

    if (ControlHeld && (Key == 's' || Key == 'S'))
    {
        if (!DiskFsIsMounted() && !DiskFsMount())
        {
            return;
        }
        if (EditorHasDir)
        {
            if (DiskFsWriteInPath(EditorDir, EditorName, EditorBuffer, EditorLength))
            {
                EditorDirty = false;
            }
        }
        else if (DiskFsWrite(EditorName, EditorBuffer, EditorLength))
        {
            EditorDirty = false;
        }
        return;
    }
    if (ControlHeld && (Key == 'q' || Key == 'Q'))
    {
        EditorRunning = false;
        TerminalClear32(EDITOR_BG);
        return;
    }

    /* Copy / paste with Ctrl+Shift+C / Ctrl+Shift+V */
    if (ControlHeld && ShiftHeld && (Key == 'c' || Key == 'C'))
    {
        uint32_t a = EditorSelMin();
        uint32_t b = EditorSelMax();
        if (a < b && (b - a) < EDITOR_CAPACITY)
        {
            uint32_t k = 0;
            for (uint32_t i = a; i < b && k < EDITOR_CAPACITY - 1U; i++)
            {
                EditorClipboard[k++] = EditorBuffer[i];
            }
            EditorClipLen = k;
        }
        return;
    }
    if (ControlHeld && ShiftHeld && (Key == 'v' || Key == 'V'))
    {
        for (uint32_t i = 0; i < EditorClipLen; i++)
        {
            EditorInsertChar(EditorClipboard[i]);
        }
        EditorClearSelection();
        EditorPaintText();
        return;
    }

    switch (Key)
    {
        case KBD_KEY_LEFT:
            if (ShiftHeld)
            {
                if (!EditorSelecting) { EditorSelAnchor = EditorPos; EditorSelecting = true; }
                if (EditorPos > 0U) EditorPos--;
            }
            else
            {
                EditorClearSelection();
                if (EditorPos > 0U) EditorPos--;
            }
            EditorClampScroll();
            EditorPaintText();
            return;
        case KBD_KEY_RIGHT:
            if (ShiftHeld)
            {
                if (!EditorSelecting) { EditorSelAnchor = EditorPos; EditorSelecting = true; }
                if (EditorPos < EditorLength) EditorPos++;
            }
            else
            {
                EditorClearSelection();
                if (EditorPos < EditorLength) EditorPos++;
            }
            EditorClampScroll();
            EditorPaintText();
            return;
        case KBD_KEY_HOME:
            if (ShiftHeld)
            {
                if (!EditorSelecting) { EditorSelAnchor = EditorPos; EditorSelecting = true; }
            }
            else
            {
                EditorClearSelection();
            }
            EditorPos = EditorLineStart(EditorPos);
            EditorClampScroll();
            EditorPaintText();
            return;
        case KBD_KEY_END:
            if (ShiftHeld)
            {
                if (!EditorSelecting) { EditorSelAnchor = EditorPos; EditorSelecting = true; }
            }
            else
            {
                EditorClearSelection();
            }
            EditorPos = EditorLineEnd(EditorPos);
            EditorClampScroll();
            EditorPaintText();
            return;
        case KBD_KEY_UP:
        {
            uint32_t Line = 0;
            uint32_t Col = 0;
            uint32_t Start;
            uint32_t End;

            if (ShiftHeld)
            {
                if (!EditorSelecting) { EditorSelAnchor = EditorPos; EditorSelecting = true; }
            }
            else
            {
                EditorClearSelection();
            }
            EditorLineCol(EditorPos, &Line, &Col);
            if (Line > 0U)
            {
                Start = EditorLineStartByLine(Line - 1U);
                End = EditorLineEnd(Start);
                EditorPos = Start + (Col < End - Start ? Col : End - Start);
            }
            else
            {
                EditorPos = 0U;
            }
            EditorClampScroll();
            EditorPaintText();
            return;
        }
        case KBD_KEY_DOWN:
        {
            uint32_t Line = 0;
            uint32_t Col = 0;
            uint32_t Start;
            uint32_t End;
            uint32_t Next;

            if (ShiftHeld)
            {
                if (!EditorSelecting) { EditorSelAnchor = EditorPos; EditorSelecting = true; }
            }
            else
            {
                EditorClearSelection();
            }
            EditorLineCol(EditorPos, &Line, &Col);
            Start = EditorLineStart(EditorPos);
            End = EditorLineEnd(EditorPos);
            Next = EditorLineStartByLine(Line + 1U);
            if (Next > EditorLength)
            {
                EditorPos = End;
            }
            else
            {
                End = EditorLineEnd(Next);
                EditorPos = Next + (Col < End - Next ? Col : End - Next);
            }
            EditorClampScroll();
            EditorPaintText();
            return;
        }
        case KBD_KEY_DELETE:
        {
            uint32_t Line = 0;
            uint32_t Col = 0;
            char Deleted;
            uint32_t sa = EditorSelMin();
            uint32_t sb = EditorSelMax();

            if (EditorSelecting && sa < sb)
            {
                for (uint32_t i = sa; i < sb; i++)
                {
                    EditorDeleteAt(sa);
                }
                EditorPos = sa;
                EditorClearSelection();
                EditorPaintText();
                return;
            }
            if (ControlHeld)
            {
                EditorLineCol(EditorPos, &Line, &Col);
                /* Delete forward to the end of the line (stop before the
                   newline so lines are not merged). Bound by EditorLength so
                   the loop can never outrun the buffer and underflow it. */
                while (EditorPos < EditorLength && EditorBuffer[EditorPos] != '\n')
                {
                    EditorDeleteAt(EditorPos);
                }
                EditorRepaintLine(Line);
                EditorRefresh();
                return;
            }
            if (EditorPos >= EditorLength)
            {
                return;
            }
            Deleted = EditorBuffer[EditorPos];
            EditorLineCol(EditorPos, &Line, &Col);
            EditorDeleteAt(EditorPos);
            if (Deleted == '\n')
            {
                EditorPaintText();
            }
            else
            {
                EditorRepaintLine(Line);
                EditorRefresh();
            }
            return;
        }
        default:
            break;
    }

    if (Key == '\b')
    {
        uint32_t Line = 0;
        uint32_t Col = 0;
        char Deleted;
        uint32_t sa = EditorSelMin();
        uint32_t sb = EditorSelMax();

        if (EditorSelecting && sa < sb)
        {
            for (uint32_t i = sa; i < sb; i++)
            {
                EditorDeleteAt(sa);
            }
            EditorPos = sa;
            EditorClearSelection();
            EditorPaintText();
            return;
        }
        EditorClearSelection();
        if (EditorPos == 0U)
        {
            return;
        }
        EditorPos--;
        Deleted = EditorBuffer[EditorPos];
        EditorLineCol(EditorPos, &Line, &Col);
        EditorDeleteAt(EditorPos);
        if (Deleted == '\n' || EditorClampScroll())
        {
            EditorPaintText();
        }
        else
        {
            EditorRepaintLine(Line);
            EditorRefresh();
        }
        return;
    }

    if (Key == '\t')
    {
        uint32_t Line = 0;
        uint32_t Col = 0;
        uint32_t sa = EditorSelMin();
        uint32_t sb = EditorSelMax();

        if (EditorSelecting && sa < sb)
        {
            for (uint32_t i = sa; i < sb; i++)
            {
                EditorDeleteAt(sa);
            }
            EditorPos = sa;
            EditorClearSelection();
        }
        EditorLineCol(EditorPos, &Line, &Col);
        for (uint32_t i = 0; i < 4U; i++)
        {
            EditorInsertChar(' ');
        }
        EditorRepaintLine(Line);
        EditorRefresh();
        return;
    }

    if (Key == '\n' || Key == '\r' || (Key >= ' ' && Key <= '~'))
    {
        uint32_t Line = 0;
        uint32_t Col = 0;
        uint32_t sa = EditorSelMin();
        uint32_t sb = EditorSelMax();

        if (EditorSelecting && sa < sb)
        {
            for (uint32_t i = sa; i < sb; i++)
            {
                EditorDeleteAt(sa);
            }
            EditorPos = sa;
            EditorClearSelection();
        }
        EditorLineCol(EditorPos, &Line, &Col);
        if (Key == '\n' || Key == '\r')
        {
            EditorInsertChar('\n');
            EditorPaintText();
        }
        else
        {
            EditorInsertChar((char)Key);
            EditorRepaintLine(Line);
            EditorRefresh();
        }
    }
}
