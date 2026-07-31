#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

void FbInit32(void);

void FbPrintPixel32(uint32_t x,
                    uint32_t y,
                    uint32_t color);

void FbCleanScreen32(uint32_t color);

void FbDrawChar32(uint8_t c,
                  uint32_t x,
                  uint32_t y,
                  uint32_t color);
void FbDrawCharScaled32(uint8_t c,
                        uint32_t x,
                        uint32_t y,
                        uint32_t scale,
                        uint32_t color);

void FbPrintString32(const char *str,
                     uint32_t x,
                     uint32_t y,
                     uint32_t color);
void FbPrintPixel32(uint32_t x, uint32_t y, uint32_t color);
void FbFillRect32(uint32_t x,
                  uint32_t y,
                  uint32_t width,
                  uint32_t height,
                  uint32_t color);
uint32_t FbGetWidth32(void);
uint32_t FbGetHeight32(void);
void FbDrawRect32(uint32_t x,
                  uint32_t y,
                  uint32_t width,
                  uint32_t height,
                  uint32_t color);
void FbCopyRect32(uint32_t SrcX,
                  uint32_t SrcY,
                  uint32_t DstX,
                  uint32_t DstY,
                  uint32_t Width,
                  uint32_t Height);
uint32_t FbReadPixel32(
    uint32_t x,
    uint32_t y);
#endif
