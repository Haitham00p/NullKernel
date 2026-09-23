/* NullOS Graphic API implementation — see graphic.h. */
#include "Graphic/graphic.h"

#include "drivers/video_framebuffer/framebuffer.h"
#include "drivers/timer/PIT.h"
#include "drivers/input/keyboard/keyboard.h"

static uint32_t g_RandState = 0x9E3779B9u;

uint32_t GfxColor(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

uint32_t GfxColorA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) |
           ((uint32_t)g << 8) | (uint32_t)b;
}

uint32_t GfxWidth(void)  { return FbGetWidth32(); }
uint32_t GfxHeight(void) { return FbGetHeight32(); }

void GfxClear(uint32_t color) { FbCleanScreen32(color); }
void GfxPresent(void)         { FbPresent32(); }

void GfxDelay(uint64_t ms) { PITSleepMS(ms); }

void GfxPixel(uint32_t x, uint32_t y, uint32_t color)
{
    FbPrintPixel32(x, y, color);
}

void GfxLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    FrDrawLine32((uint32_t)x0, (uint32_t)y0, (uint32_t)x1, (uint32_t)y1, color);
}

void GfxRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    FbDrawRect32(x, y, w, h, color);
}

void GfxFill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    FbFillRect32(x, y, w, h, color);
}

void GfxCircle(int32_t cx, int32_t cy, int32_t r, uint32_t color)
{
    int32_t x = r;
    int32_t y = 0;
    int32_t err = 1 - r;
    while (x >= y)
    {
        FbPrintPixel32((uint32_t)(cx + x), (uint32_t)(cy + y), color);
        FbPrintPixel32((uint32_t)(cx + y), (uint32_t)(cy + x), color);
        FbPrintPixel32((uint32_t)(cx - y), (uint32_t)(cy + x), color);
        FbPrintPixel32((uint32_t)(cx - x), (uint32_t)(cy + y), color);
        FbPrintPixel32((uint32_t)(cx - x), (uint32_t)(cy - y), color);
        FbPrintPixel32((uint32_t)(cx - y), (uint32_t)(cy - x), color);
        FbPrintPixel32((uint32_t)(cx + y), (uint32_t)(cy - x), color);
        FbPrintPixel32((uint32_t)(cx + x), (uint32_t)(cy - y), color);
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* tiny integer sqrt for the fill-circle scanline */
static uint32_t fx_sqrt(uint32_t n)
{
    uint32_t r = 0, b = 0x40000000u;
    while (b > 0)
    {
        uint32_t t = r + b;
        if (t <= n) { n -= t; r = t + b; }
        r >>= 1;
        b >>= 2;
    }
    return r;
}

void GfxFillCircle(int32_t cx, int32_t cy, int32_t r, uint32_t color)
{
    for (int32_t dy = -r; dy <= r; dy++)
    {
        int32_t dx = (int32_t)(r * r - dy * dy);
        if (dx < 0) dx = 0;
        dx = (int32_t)fx_sqrt((uint32_t)dx);
        FbFillRect32((uint32_t)(cx - dx), (uint32_t)(cy + dy),
                     (uint32_t)(2 * dx), 1, color);
    }
}

uint32_t GfxGetPixel(uint32_t x, uint32_t y)
{
    return FbReadPixel32(x, y);
}

void GfxText(uint32_t x, uint32_t y, const char *str, uint32_t color)
{
    FbPrintString32(str, x, y, color);
}

void GfxTextScale(uint32_t x, uint32_t y, const char *str,
                  uint32_t scale, uint32_t color)
{
    uint32_t cx = x;
    while (str && *str)
    {
        FbDrawCharScaled32((uint8_t)*str, cx, y, scale, color);
        cx += 8u * scale;
        str++;
    }
}

uint32_t GfxRandom(void)
{
    /* xorshift32 */
    uint32_t x = g_RandState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_RandState = x;
    return x;
}

int32_t GfxRandRange(int32_t min, int32_t max)
{
    if (max <= min) return min;
    return (int32_t)(GfxRandom() % (uint32_t)(max - min)) + min;
}

uint32_t GfxMin(uint32_t a, uint32_t b) { return a < b ? a : b; }
uint32_t GfxMax(uint32_t a, uint32_t b) { return a > b ? a : b; }
int32_t  GfxAbs(int32_t v) { return v < 0 ? -v : v; }

void GfxBlit(uint32_t x, uint32_t y, const uint32_t *pixels,
             uint32_t w, uint32_t h)
{
    uint32_t i = 0;
    for (uint32_t dy = 0; dy < h; dy++)
    {
        for (uint32_t dx = 0; dx < w; dx++)
        {
            uint32_t p = pixels[i++];
            if ((p >> 24) != 0u) /* skip fully transparent */
                FbPrintPixel32(x + dx, y + dy, p & 0x00FFFFFFu);
        }
    }
}

uint8_t GfxGetKey(void)
{
    if (KbdAvailable())
        return (uint8_t)KbdReadChar();
    return 0;
}

bool GfxAbort(void)
{
    return KbdAbortRequested();
}
