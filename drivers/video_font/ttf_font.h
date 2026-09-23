#ifndef TTF_FONT_H
#define TTF_FONT_H

#include <stdint.h>

/*
 * Builds the existing 128x16 terminal bitmap from the embedded
 * IBM Plex Mono Regular TrueType file. The public terminal API is
 * deliberately untouched.
 */
void TtfFontInit(void);

#define TTF_FONT_WIDTH  16U
#define TTF_FONT_HEIGHT 32U

const uint8_t *TtfFontGetAtlas(void);

#endif
