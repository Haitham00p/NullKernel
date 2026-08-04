#ifndef NULLOS_CURSOR_H
#define NULLOS_CURSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TERM_ATTR_NONE          (0U)
#define TERM_ATTR_BOLD          (1U << 0)
#define TERM_ATTR_ITALIC        (1U << 1)
#define TERM_ATTR_UNDERLINE     (1U << 2)
#define TERM_ATTR_BLINK         (1U << 3)
#define TERM_ATTR_INVERSE       (1U << 4)
#define TERM_ATTR_STRIKETHROUGH (1U << 5)
#define TERM_ATTR_PROTECTED     (1U << 6)
#define TERM_ATTR_DIRTY         (1U << 7)
#define TERM_ATTR_SELECTED      (1U << 8)

typedef uint16_t term_attr_t;

typedef struct __attribute__((aligned(16))) {
    uint32_t codepoint;
    uint32_t fg_color;
    uint32_t bg_color;
    term_attr_t attributes;
    uint8_t  link_id;
    uint8_t  reserved;
} term_cell_t;

typedef struct {
    term_cell_t *cells;
    uint32_t     length;
    bool         is_dirty;
    uint16_t     first_dirty;
    uint16_t     last_dirty;
} term_line_t;

typedef enum {
    CURSOR_SHAPE_WIDE_CARET = 0,
    CURSOR_SHAPE_THIN_CARET,
    CURSOR_SHAPE_BLOCK,
    CURSOR_SHAPE_HOLLOW_BLOCK,
    CURSOR_SHAPE_UNDERLINE,
    CURSOR_SHAPE_CUSTOM
} cursor_shape_t;

typedef enum {
    CURSOR_RENDER_GEOMETRIC_OVERLAY = 0,
    CURSOR_RENDER_INVERT_COLORS,
    CURSOR_RENDER_TINT_BACKGROUND,
    CURSOR_RENDER_ALPHA_BLEND
} cursor_render_mode_t;

typedef struct __attribute__((aligned(64))) cursor_s {
    uint32_t col;
    uint32_t row;
    uint32_t preferred_col;

    uint32_t pixel_x;
    uint32_t pixel_y;
    uint8_t  width_px;
    uint8_t  height_px;

    uint32_t color;
    uint32_t accent_color;
    cursor_shape_t shape;
    cursor_render_mode_t render_mode;

    bool visible;
    bool blink_state;
    bool blink_enabled;
    bool overwrite_mode;

    uint64_t blink_period_ms;
    uint64_t last_activity_ms;

    struct cursor_s *next_cursor;
    uint32_t id;
} cursor_t;

typedef struct {
    term_line_t *lines;
    uint32_t     cols;
    uint32_t     rows;

    const uint8_t *font_glyph_data;
    uint8_t      font_width;
    uint8_t      font_height;

    uint32_t    *framebuffer;
    uint32_t     fb_pitch_pixels;
    uint32_t     fb_width;
    uint32_t     fb_height;

    cursor_t     primary_cursor;
} term_screen_t;

bool TerminalInitialize(term_screen_t *screen, uint32_t cols, uint32_t rows,
                        uint32_t *framebuffer, uint32_t fb_width, 
                        uint32_t fb_height, uint32_t fb_pitch_pixels,
                        const uint8_t *font_atlas, uint8_t font_w, uint8_t font_h);

void CursorInitialize(cursor_t *cursor, uint32_t id);
void CursorMoveTo(term_screen_t *screen, uint32_t col, uint32_t row);
void CursorMoveLeft(term_screen_t *screen, uint32_t delta);
void CursorMoveRight(term_screen_t *screen, uint32_t delta);
void CursorMoveUp(term_screen_t *screen, uint32_t delta);
void CursorMoveDown(term_screen_t *screen, uint32_t delta);

void CursorSetShape(term_screen_t *screen, cursor_shape_t shape, uint8_t width_px);
void CursorSetColor(term_screen_t *screen, uint32_t color, uint32_t accent_color);
void CursorSetVisible(term_screen_t *screen, bool visible);
void CursorSetMode(term_screen_t *screen, bool overwrite_mode);

void TerminalInvalidateCell(term_screen_t *screen, uint32_t col, uint32_t row);
void TerminalInvalidateAll(term_screen_t *screen);
void CursorBlinkTick(term_screen_t *screen, uint64_t current_time_ms);
void TerminalRender(term_screen_t *screen);

void TerminalPutChar(term_screen_t *screen, uint32_t codepoint, uint32_t fg, uint32_t bg);
void TerminalBackspace(term_screen_t *screen);
void TerminalDelete(term_screen_t *screen);

#endif /* NULLOS_CURSOR_H */
