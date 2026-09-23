#include "cursor.h"
#include "drivers/timer/PIT.h"
#include "drivers/video_framebuffer/framebuffer.h"

static inline void update_dirty_range(term_line_t *line, uint32_t col) {
    line->is_dirty = true;
    if (col < line->first_dirty) line->first_dirty = (uint16_t)col;
    if (col > line->last_dirty)  line->last_dirty  = (uint16_t)col;
}

void CursorInitialize(cursor_t *cursor, uint32_t id) {
    if (!cursor) return;

    cursor->col = 0;
    cursor->row = 0;
    cursor->preferred_col = 0;

    cursor->pixel_x = 0;
    cursor->pixel_y = 0;
    cursor->width_px = 14;
    cursor->height_px = 28;

    cursor->color = 0x0000E0FF;        /* Bright Cyan */
    cursor->accent_color = 0x00000000; /* Black contrast for glyph intersection */
    cursor->shape = CURSOR_SHAPE_WIDE_CARET;
    cursor->render_mode = CURSOR_RENDER_INVERT_COLORS;

    cursor->visible = true;
    cursor->blink_state = true;
    cursor->blink_enabled = true;
    cursor->overwrite_mode = false;

    cursor->blink_period_ms = 500;     /* Toggle every 500ms (0.5 sec) */
    cursor->last_activity_ms = 0;

    cursor->next_cursor = NULL;
    cursor->id = id;
}

bool TerminalInitialize(term_screen_t *screen, uint32_t cols, uint32_t rows,
                        uint32_t *framebuffer, uint32_t fb_width, 
                        uint32_t fb_height, uint32_t fb_pitch_pixels,
                        const uint8_t *font_atlas, uint8_t font_w, uint8_t font_h) {
    if (!screen || !framebuffer) return false;

    screen->cols = cols;
    screen->rows = rows;
    screen->framebuffer = framebuffer;
    screen->fb_width = fb_width;
    screen->fb_height = fb_height;
    screen->fb_pitch_pixels = fb_pitch_pixels;
    screen->font_glyph_data = font_atlas;
    screen->font_width = font_w;
    screen->font_height = font_h;

    CursorInitialize(&screen->primary_cursor, 0);
    screen->primary_cursor.width_px = font_w;  /* Wide enough to fit whole letter */
    screen->primary_cursor.height_px = font_h;

    return true;
}

void TerminalInvalidateCell(term_screen_t *screen, uint32_t col, uint32_t row) {
    if (!screen || row >= screen->rows || col >= screen->cols) return;

    term_line_t *line = &screen->lines[row];
    line->cells[col].attributes |= TERM_ATTR_DIRTY;
    update_dirty_range(line, col);
}

void TerminalInvalidateAll(term_screen_t *screen) {
    if (!screen) return;

    for (uint32_t r = 0; r < screen->rows; r++) {
        term_line_t *line = &screen->lines[r];
        line->is_dirty = true;
        line->first_dirty = 0;
        line->last_dirty = (uint16_t)(screen->cols - 1);
        for (uint32_t c = 0; c < screen->cols; c++) {
            line->cells[c].attributes |= TERM_ATTR_DIRTY;
        }
    }
}

void CursorMoveTo(term_screen_t *screen, uint32_t col, uint32_t row) {
    if (!screen) return;

    if (col >= screen->cols) col = screen->cols - 1;
    if (row >= screen->rows) row = screen->rows - 1;

    cursor_t *cursor = &screen->primary_cursor;

    /* Invalidate previous position */
    TerminalInvalidateCell(screen, cursor->col, cursor->row);

    /* Update coordinates */
    cursor->col = col;
    cursor->row = row;
    cursor->preferred_col = col;
    cursor->pixel_x = col * screen->font_width;
    cursor->pixel_y = row * screen->font_height;

    /* Reset blink phase on movement */
    cursor->blink_state = true;
    cursor->last_activity_ms = PITGetMilliseconds();

    /* Invalidate new position */
    TerminalInvalidateCell(screen, cursor->col, cursor->row);
}

void CursorMoveLeft(term_screen_t *screen, uint32_t delta) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    uint32_t new_col = (c->col >= delta) ? (c->col - delta) : 0;
    CursorMoveTo(screen, new_col, c->row);
}

void CursorMoveRight(term_screen_t *screen, uint32_t delta) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    uint32_t new_col = c->col + delta;
    if (new_col >= screen->cols) new_col = screen->cols - 1;
    CursorMoveTo(screen, new_col, c->row);
}

void CursorMoveUp(term_screen_t *screen, uint32_t delta) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    uint32_t new_row = (c->row >= delta) ? (c->row - delta) : 0;
    uint32_t target_col = (c->preferred_col < screen->cols) ? c->preferred_col : (screen->cols - 1);
    
    TerminalInvalidateCell(screen, c->col, c->row);
    c->row = new_row;
    c->col = target_col;
    c->pixel_x = c->col * screen->font_width;
    c->pixel_y = c->row * screen->font_height;
    c->blink_state = true;
    c->last_activity_ms = PITGetMilliseconds();
    TerminalInvalidateCell(screen, c->col, c->row);
}

void CursorMoveDown(term_screen_t *screen, uint32_t delta) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    uint32_t new_row = c->row + delta;
    if (new_row >= screen->rows) new_row = screen->rows - 1;
    uint32_t target_col = (c->preferred_col < screen->cols) ? c->preferred_col : (screen->cols - 1);

    TerminalInvalidateCell(screen, c->col, c->row);
    c->row = new_row;
    c->col = target_col;
    c->pixel_x = c->col * screen->font_width;
    c->pixel_y = c->row * screen->font_height;
    c->blink_state = true;
    c->last_activity_ms = PITGetMilliseconds();
    TerminalInvalidateCell(screen, c->col, c->row);
}

void CursorSetShape(term_screen_t *screen, cursor_shape_t shape, uint8_t width_px) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    c->shape = shape;
    if (width_px > 0) c->width_px = width_px;
    TerminalInvalidateCell(screen, c->col, c->row);
}

void CursorSetColor(term_screen_t *screen, uint32_t color, uint32_t accent_color) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    c->color = color;
    c->accent_color = accent_color;
    TerminalInvalidateCell(screen, c->col, c->row);
}

void CursorSetVisible(term_screen_t *screen, bool visible) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    if (c->visible != visible) {
        c->visible = visible;
        TerminalInvalidateCell(screen, c->col, c->row);
    }
}

void CursorSetMode(term_screen_t *screen, bool overwrite_mode) {
    if (!screen) return;
    screen->primary_cursor.overwrite_mode = overwrite_mode;
}

void CursorBlinkTick(term_screen_t *screen, uint64_t current_time_ms) {
    if (!screen) return;
    cursor_t *c = &screen->primary_cursor;
    if (!c->visible || !c->blink_enabled) return;

    if (current_time_ms - c->last_activity_ms >= c->blink_period_ms) {
        c->blink_state = !c->blink_state;
        c->last_activity_ms = current_time_ms;
        TerminalInvalidateCell(screen, c->col, c->row);
    }
}

static inline void RenderSingleCell(term_screen_t *screen, uint32_t col, uint32_t row) {
    term_cell_t *cell = &screen->lines[row].cells[col];
    cursor_t *cursor = &screen->primary_cursor;

    bool is_cursor = cursor->visible && cursor->blink_state && 
                     (cursor->col == col) && (cursor->row == row);

    uint8_t fw = screen->font_width;
    uint8_t fh = screen->font_height;
    uint32_t pitch = screen->fb_pitch_pixels;

    uint32_t fg = (cell->attributes & TERM_ATTR_INVERSE) ? cell->bg_color : cell->fg_color;
    uint32_t bg = (cell->attributes & TERM_ATTR_INVERSE) ? cell->fg_color : cell->bg_color;

    /* Dynamic color inversion using bitwise NOT (~) read from cell background and letter colors */
    uint32_t fg_inverted = (~fg) | 0xFF000000U;
    uint32_t bg_inverted = (~bg) | 0xFF000000U;

    const uint8_t *glyph_ptr = NULL;
    if (screen->font_glyph_data && cell->codepoint < 256) {
        glyph_ptr = &screen->font_glyph_data[cell->codepoint * (uint32_t)fh * 2U];
    }

    uint32_t start_x = col * fw;
    uint32_t start_y = row * fh;

    /*
     * Cell geometry normally fits exactly because TerminalInit32 derives
     * cols/rows with integer division. Keep the renderer defensive anyway:
     * editor/status/cursor code must never write outside the framebuffer.
     */
    for (uint8_t py = 0; py < fh; py++) {
        uint32_t y = start_y + (uint32_t)py;
        if (y >= screen->fb_height)
            continue;

        for (uint8_t px = 0; px < fw; px++) {
            uint32_t x = start_x + (uint32_t)px;
            if (x >= screen->fb_width)
                continue;

            uint32_t *vram_pixel = &screen->framebuffer[y * pitch + x];
            /*
             * Atlas rows are packed MSB-first into ceil(fw/8) bytes.
             * Do not assemble a 14-bit row into the low bits of a uint16_t:
             * that shifts every glyph horizontally and can make narrow
             * glyphs disappear.
             */
            bool is_glyph = false;
            if (glyph_ptr) {
                uint32_t bytes_per_row = ((uint32_t)fw + 7U) / 8U;
                uint8_t glyph_byte = glyph_ptr[(uint32_t)py * bytes_per_row +
                                              ((uint32_t)px >> 3)];
                is_glyph = (glyph_byte & (uint8_t)(1U << (7U - (px & 7U)))) != 0;
            }
            bool is_caret = false;

            if (is_cursor) {
                switch (cursor->shape) {
                    case CURSOR_SHAPE_WIDE_CARET:
                    case CURSOR_SHAPE_BLOCK:
                        is_caret = (px < cursor->width_px);
                        break;
                    case CURSOR_SHAPE_THIN_CARET:
                        is_caret = (px < 2);
                        break;
                    case CURSOR_SHAPE_HOLLOW_BLOCK:
                        is_caret = (px == 0 || px == (fw - 1) || py == 0 || py == (fh - 1));
                        break;
                    case CURSOR_SHAPE_UNDERLINE:
                        is_caret = (py >= (fh - 2));
                        break;
                    default:
                        is_caret = (px < cursor->width_px);
                        break;
                }
            }

            uint32_t final_pixel;
            if (is_cursor && is_caret) {
                /* Invert both background and letter color when cursor appears */
                final_pixel = is_glyph ? fg_inverted : bg_inverted;
            } else {
                /* Normal rendering when cursor disappears or off cursor */
                final_pixel = is_glyph ? fg : bg;
            }

            *vram_pixel = final_pixel;
        }
    }

    /* The cell was written straight into the back buffer, so tell the present
       path that this region changed. Without this the periodic blit never
       picks up terminal output and the display freezes on the last present. */
    if (start_x < screen->fb_width && start_y < screen->fb_height) {
        uint32_t dirty_x1 = start_x + (uint32_t)fw - 1U;
        uint32_t dirty_y1 = start_y + (uint32_t)fh - 1U;
        if (dirty_x1 >= screen->fb_width) dirty_x1 = screen->fb_width - 1U;
        if (dirty_y1 >= screen->fb_height) dirty_y1 = screen->fb_height - 1U;
        FbMarkDirty(start_x, start_y, dirty_x1, dirty_y1);
    }

    cell->attributes &= ~TERM_ATTR_DIRTY;
}

void TerminalRender(term_screen_t *screen) {
    if (!screen || !screen->framebuffer) return;

    for (uint32_t r = 0; r < screen->rows; r++) {
        term_line_t *line = &screen->lines[r];
        if (!line->is_dirty) continue;

        for (uint32_t c = line->first_dirty; c <= line->last_dirty && c < screen->cols; c++) {
            if (line->cells[c].attributes & TERM_ATTR_DIRTY) {
                RenderSingleCell(screen, c, r);
            }
        }

        line->is_dirty = false;
        line->first_dirty = (uint16_t)(screen->cols - 1);
        line->last_dirty = 0;
    }
}
