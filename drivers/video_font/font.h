#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/*
 * Terminal font cell geometry.
 *
 * The glyph atlas is backed by IBM Plex Mono Regular, rasterized into the
 * existing 8x16 cells. Keep these values stable: terminal callers and the
 * Terminal API do not depend on the font implementation.
 */
#define FONT_WIDTH   8
#define FONT_HEIGHT 16

extern unsigned char font8x16[128][FONT_HEIGHT];

#endif