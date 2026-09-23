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
    /*
     * Freestanding kernel: there is no libm, so do not use __builtin_sqrt()
     * here because GCC may lower it to an external sqrt() symbol.
     */
    if (x <= 0.0)
        return 0.0;

    double lo = 0.0;
    double hi = (x < 1.0) ? 1.0 : x;

    for (int i = 0; i < 64; ++i) {
        double mid = (lo + hi) * 0.5;
        if (mid * mid > x)
            hi = mid;
        else
            lo = mid;
    }

    return (lo + hi) * 0.5;
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
#define STBTT_sqrt(x)     ttf_sqrt(x)
/* This vendored stb_truetype version calls sqrt() directly. */
#define sqrt(x)            ttf_sqrt(x)

#include "drivers/video_font/stb_truetype.h"
#undef sqrt

static uint8_t terminal_font16x32[128][TTF_FONT_HEIGHT * 2U];
static unsigned char ttf_bitmap[64U * 64U];

static void clear_glyph(uint8_t out[TTF_FONT_HEIGHT * 2U])
{
    for (uint32_t i = 0; i < TTF_FONT_HEIGHT * 2U; ++i)
        out[i] = 0;
}

static void set_glyph_pixel(uint8_t out[TTF_FONT_HEIGHT * 2U],
                             int x, int y)
{
    if (x < 0 || x >= (int)TTF_FONT_WIDTH ||
        y < 0 || y >= (int)TTF_FONT_HEIGHT)
        return;

    uint32_t bit = (uint32_t)(7 - (x & 7));
    uint32_t byte = (uint32_t)y * 2U + (uint32_t)(x >> 3);
    out[byte] |= (uint8_t)(1U << bit);
}

static void bake_codepoint(const stbtt_fontinfo *font,
                           int codepoint,
                           float scale,
                           uint8_t out[TTF_FONT_HEIGHT * 2U])
{
    /*
     * Render into a static scratch buffer. Do not put this bitmap on the
     * kernel stack: 64x64 bytes is large enough to damage a small boot stack.
     */
    enum { BITMAP_MAX = 64 };
    unsigned char *bitmap = ttf_bitmap;

    clear_glyph(out);

    int ix0, iy0, ix1, iy1;
    stbtt_GetCodepointBitmapBox(font, codepoint, scale, scale,
                                &ix0, &iy0, &ix1, &iy1);

    int w = ix1 - ix0;
    int h = iy1 - iy0;

    if (w <= 0 || h <= 0 || w > BITMAP_MAX || h > BITMAP_MAX)
        return;

    for (int i = 0; i < w * h; ++i)
        bitmap[i] = 0;

    stbtt_MakeCodepointBitmap(font, bitmap, w, h, w,
                              scale, scale, codepoint);

    int dst_x = ((int)TTF_FONT_WIDTH - w) / 2;
    int dst_y = ((int)TTF_FONT_HEIGHT - h) / 2;

    for (int sy = 0; sy < h; ++sy) {
        int dy = dst_y + sy;
        if (dy < 0 || dy >= (int)TTF_FONT_HEIGHT)
            continue;

        for (int sx = 0; sx < w; ++sx) {
            int dx = dst_x + sx;

            if (dx < 0)
                dx = sx - (w - (int)TTF_FONT_WIDTH) / 2;

            if (bitmap[sy * w + sx] != 0)
                set_glyph_pixel(out, dx, dy);
        }
    }
}


static void build_fallback_atlas(void)
{
    for (uint32_t cp = 0; cp < 128U; ++cp) {
        for (uint32_t y = 0; y < FONT_HEIGHT; ++y) {
            uint8_t row = font8x16[cp][y];

            for (uint32_t sy = 0; sy < 2U; ++sy) {
                uint32_t dy = y * 2U + sy;
                terminal_font16x32[cp][dy * 2U] = 0;
                terminal_font16x32[cp][dy * 2U + 1U] = 0;

                for (uint32_t x = 0; x < FONT_WIDTH; ++x) {
                    if (!(row & (uint8_t)(1U << (7U - x))))
                        continue;

                    uint32_t dx = x * 2U;
                    uint32_t byte = dy * 2U + (dx >> 3);
                    uint32_t bit = 7U - (dx & 7U);
                    terminal_font16x32[cp][byte] |= (uint8_t)(1U << bit);
                    bit = 7U - ((dx + 1U) & 7U);
                    terminal_font16x32[cp][dy * 2U + ((dx + 1U) >> 3)] |=
                        (uint8_t)(1U << bit);
                }
            }
        }
    }
}

const uint8_t *TtfFontGetAtlas(void)
{
    return &terminal_font16x32[0][0];
}

void TtfFontInit(void)
{
    build_fallback_atlas();

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

    /* Rasterize IBM Plex Mono into the current 14x28 terminal cells. */
    float scale = stbtt_ScaleForPixelHeight(&font, 30.0f);

    for (int cp = 32; cp < 128; ++cp) {
        uint8_t glyph[TTF_FONT_HEIGHT * 2U];
        bake_codepoint(&font, cp, scale, glyph);

        bool has_pixels = false;
        for (uint32_t i = 0; i < TTF_FONT_HEIGHT * 2U; ++i) {
            if (glyph[i] != 0) {
                has_pixels = true;
                break;
            }
        }

        if (has_pixels) {
            for (uint32_t i = 0; i < TTF_FONT_HEIGHT * 2U; ++i)
                terminal_font16x32[cp][i] = glyph[i];
        }
    }
}
