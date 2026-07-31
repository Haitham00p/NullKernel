#include "framebuffer.h"
#include "drivers/video_font/font.h"
#include "lib/delay/delay.h"
#include "vendor/limine/limine.h"
#include <stdint.h>
#include <stddef.h>
#define BytePerPixel 4

__attribute__((used, section(".requests"))) volatile struct limine_memmap_request MemMapReq = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0};


__attribute__((used, section(".requests"))) static volatile struct limine_framebuffer_request FrBuf32 = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0};

static struct limine_framebuffer *framebuffer;


static uint32_t *fb_address;
static uint64_t fb_width;
static uint64_t fb_height;
uint64_t fb_pitch;
static uint16_t fb_bpp;

void FbInit32(void)
{
    if (FrBuf32.response == NULL)
    {
        return;
    }
    
    framebuffer =
        FrBuf32.response->framebuffers[0];

    fb_address =
        (uint32_t *)framebuffer->address;

    fb_width =
        framebuffer->width;

    fb_height =
        framebuffer->height;

    fb_pitch =
        framebuffer->pitch;

    fb_bpp =
        framebuffer->bpp;
}


void FbPrintPixel32(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t *pixel = (uint32_t *)((uint8_t *)fb_address + y * fb_pitch + x * BytePerPixel);
    *pixel = color;
}

void FbCleanScreen32(uint32_t color)
{
    for (uint32_t y = 0; y < fb_height; y++)
    {
        for (uint32_t x = 0; x < fb_width; x++)
        {
            FbPrintPixel32(x, y, color);
        }
    }
}

void FbDrawChar32(uint8_t c,
                  uint32_t x,
                  uint32_t y,
                  uint32_t color)
{
    for (uint32_t row = 0; row < FONT_HEIGHT; row++)
    {
        uint8_t line = font8x16[c][row];

        for (uint32_t col = 0; col < FONT_WIDTH; col++)
        {
            if (line & (0x80 >> col))
            {
                FbPrintPixel32(x + col,
                               y + row,
                               color);
            }
        }
    }
}

void FbDrawCharScaled32(uint8_t c,
                        uint32_t x,
                        uint32_t y,
                        uint32_t scale,
                        uint32_t color)
{
    if (scale == 0) return;

    for (uint32_t Row = 0; Row < FONT_HEIGHT; Row++)
    {
        uint8_t Line = font8x16[c][Row];
        for (uint32_t Col = 0; Col < FONT_WIDTH; Col++)
        {
            if (Line & (0x80 >> Col))
            {
                FbFillRect32(x + Col * scale, y + Row * scale,
                             scale, scale, color);
            }
        }
    }
}

void FbPrintString32(const char *str,
                     uint32_t x,
                     uint32_t y,
                     uint32_t color)
{
    while (*str)
    {
        if (*str == '\n')
        {
            x = 0;
            y += FONT_HEIGHT;
            str++;
            continue;
        }

        if (*str == '\t')
        {
            x += FONT_WIDTH * 4;
            str++;
            continue;
        }

        if (x + FONT_WIDTH >= fb_width)
        {
            x = 0;
            y += FONT_HEIGHT;
        }

        FbDrawChar32((uint8_t)*str,
                     x,
                     y,
                     color);

        x += FONT_WIDTH;
        str++;
    }
}

void FbFillRect32(uint32_t x,
                  uint32_t y,
                  uint32_t width,
                  uint32_t height,
                  uint32_t color)
{
    for (uint32_t Row = 0; Row < height; Row++)
    {
        for (uint32_t Col = 0; Col < width; Col++)
        {
            FbPrintPixel32(x + Col,
                           y + Row,
                           color);
        }
    }
}

uint32_t FbGetWidth32(void)
{
    return fb_width;
}

uint32_t FbGetHeight32(void)
{
    return fb_height;
}

void FbDrawRect32(uint32_t x,
                  uint32_t y,
                  uint32_t width,
                  uint32_t height,
                  uint32_t color)
{
    for (uint32_t i = 0; i < width; i++)
    {
        FbPrintPixel32(x + i, y, color);
        FbPrintPixel32(x + i, y + height - 1, color);
    }

    for (uint32_t i = 0; i < height; i++)
    {
        FbPrintPixel32(x, y + i, color);
        FbPrintPixel32(x + width - 1, y + i, color);
    }
}

uint32_t FbReadPixel32(uint32_t x, uint32_t y)
{
    uint32_t *pixel =
        (uint32_t *)((uint8_t *)fb_address +
        y * fb_pitch +
        x * BytePerPixel);

    return *pixel;
}

void FbCopyRect32(uint32_t SrcX,
                  uint32_t SrcY,
                  uint32_t DstX,
                  uint32_t DstY,
                  uint32_t Width,
                  uint32_t Height)
{
    for (uint32_t Row = 0; Row < Height; Row++)
    {
        for (uint32_t Col = 0; Col < Width; Col++)
        {
            uint32_t Color =
                FbReadPixel32(SrcX + Col,
                              SrcY + Row);

            FbPrintPixel32(DstX + Col,
                           DstY + Row,
                           Color);
        }
    }
}
