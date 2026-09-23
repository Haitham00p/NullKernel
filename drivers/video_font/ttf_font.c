#include "ttf_font.h"
#include "drivers/video_font/font.h"
#include "libc/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

extern const unsigned char _binary_drivers_video_font_IBMPlexMono_Regular_ttf_start[];
extern const unsigned char _binary_drivers_video_font_IBMPlexMono_Regular_ttf_end[];

/*
 * IBM Plex Mono -> existing NullOS 8x16 terminal atlas.
 *
 * The terminal API is intentionally untouched. We only replace the
 * contents of font8x16[] during TtfFontInit().
 *
 * stb_truetype is used here because the previous hand-written contour
 * rasterizer was too easy to get subtly wrong. It handles TrueType
 * contour walking, quadratic curves and compound glyphs.
 */

static int ttf_ifloor(double x)
{
    int i = (int)x;
    if ((double)i > x) --i;
    return i;
}

static int ttf_iceil(double x)
{
    int i = (int)x;
    if ((double)i < x) ++i;
    return i;
}

static double ttf_sqrt(double x)
{
    if (x <= 0.0) return 0.0;
    return __builtin_sqrt(x);
}

/*
 * NullOS already provides malloc/free/qsort/memcpy/memset.
 * Override stb_truetype's hosted-runtime hooks so it stays compatible
 * with the freestanding kernel build.
 */
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_ifloor(x) ttf_ifloor(x)
#define STBTT_iceil(x)  ttf_iceil(x)
#define STBTT_malloc(x,u) malloc(x)
#define STBTT_free(x,u)   free(x)
#define STBTT_assert(x)   ((void)0)
#define sqrt(x)           ttf_sqrt(x)

#include "drivers/video_font/stb_truetype.h"

static void clear_glyph(uint8_t out[FONT_HEIGHT])
{
    for (uint32_t y = 0; y < FONT_HEIGHT; ++y)
        out[y] = 0;
}

static void bake_codepoint(const stbtt_fontinfo *font,
                           int codepoint,
                           float scale,
                           uint8_t out[FONT_HEIGHT])
{
    int w, h, xoff, yoff;
    unsigned char *bitmap;

    clear_glyph(out);

    bitmap = stbtt_GetCodepointBitmap(font, scale, scale,
                                      codepoint,
                                      &w, &h, &xoff, &yoff);
    if (!bitmap || w <= 0 || h <= 0)
        return;

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &line_gap);
    (void)descent;
    (void)line_gap;
    (void)xoff;

    int baseline = ttf_ifloor((double)ascent * (double)scale + 0.5);
    int dst_x = (FONT_WIDTH - w) / 2;

    if (dst_x < 0)
        dst_x = 0;

    for (int sy = 0; sy < h; ++sy) {
        int dy = baseline + yoff + sy;
        if (dy < 0 || dy >= FONT_HEIGHT)
            continue;

        for (int sx = 0; sx < w; ++sx) {
            int dx;

            if (w <= FONT_WIDTH)
                dx = dst_x + sx;
            else
                dx = sx - (w - FONT_WIDTH) / 2;

            if (dx < 0 || dx >= FONT_WIDTH)
                continue;

            /*
             * The terminal atlas is 1bpp. A slightly lower threshold keeps
             * thin IBM Plex Mono strokes visible at 8x16.
             */
            if (bitmap[sy * w + sx] >= 96)
                out[dy] |= (uint8_t)(1U << (7U - (uint32_t)dx));
        }
    }

    stbtt_FreeBitmap(bitmap, NULL);
}

void TtfFontInit(void)
{
    const unsigned char *ttf =
        _binary_drivers_video_font_IBMPlexMono_Regular_ttf_start;

    uint32_t ttf_size =
        (uint32_t)(_binary_drivers_video_font_IBMPlexMono_Regular_ttf_end -
                   _binary_drivers_video_font_IBMPlexMono_Regular_ttf_start);

    if (ttf_size < 4)
        return;

    stbtt_fontinfo font;

    if (!stbtt_InitFont(&font, ttf,
                        stbtt_GetFontOffsetForIndex(ttf, 0)))
        return;

    /*
     * 15px gives IBM Plex Mono enough vertical room while leaving the
     * existing 16px terminal cell intact.
     */
    float scale = stbtt_ScaleForPixelHeight(&font, 15.0f);

    for (int cp = 32; cp < 128; ++cp) {
        uint8_t glyph[FONT_HEIGHT];

        bake_codepoint(&font, cp, scale, glyph);

        bool has_pixels = false;
        for (uint32_t y = 0; y < FONT_HEIGHT; ++y) {
            if (glyph[y] != 0) {
                has_pixels = true;
                break;
            }
        }

        /*
         * Keep the old bitmap for unsupported/empty entries.
         */
        if (has_pixels) {
            for (uint32_t y = 0; y < FONT_HEIGHT; ++y)
                font8x16[cp][y] = glyph[y];
        }
    }
}
