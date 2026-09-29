#include "framebuffer.h"
#include "drivers/video_font/font.h"
#include "lib/delay/delay.h"
#include "include/nullboot.h"
#include "mm/heap/heap.h"
#include "mm/vmm.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define BytePerPixel 4

static uint32_t *fb_address;   /* hardware front buffer (what the display scans) */
static uint32_t *fb_back;      /* off-screen back buffer used for compositing   */
static uint32_t *fb_active;    /* the buffer draw calls currently target        */
static uint64_t fb_width;
static uint64_t fb_height;
uint64_t fb_pitch;
static uint16_t fb_bpp;

/* Dirty-rectangle tracking so the periodic present only blits what changed.
   A full-frame blit inside the 1 kHz timer IRQ overruns the tick period and
   starves the rest of the system (the OS appears frozen), so we accumulate
   the changed region and copy just that. */
static bool g_dirty = false;
static uint32_t g_dx0, g_dy0, g_dx1, g_dy1;

void FbMarkDirty(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);

static bool FbMapHardwareBuffer(void)
{
    return framebuffer != NULL && framebuffer->Address != 0;
}

void FbInit32(void)
{
    if (NullBoot == NULL || NullBoot->Framebuffer == 0)
        return;

    framebuffer = (NullBootFramebuffer *)(uintptr_t)NullBoot->Framebuffer;
    if (framebuffer->Address == 0 || framebuffer->Width == 0 || framebuffer->Height == 0)
        return;

    fb_address = (uint32_t *)(uintptr_t)framebuffer->Address;
    fb_width = framebuffer->Width;
    fb_height = framebuffer->Height;
    fb_pitch = framebuffer->Pitch;
    fb_bpp = framebuffer->Bpp;
    fb_active = fb_address;
}

/* Return the Limine HHDM offset used to translate physical addresses into
   directly mapped kernel virtual addresses. */
uint64_t FbGetHhdmOffset32(void)
{
    return 0;
}

/* Allocate the off-screen back buffer and switch compositing to it. Must be
   called AFTER HeapInitialize() because it kmalloc()s a full-screen buffer.
   Runs front->back copy so the first present shows the current display. */
void FbInitDoubleBuffer(void)
{
    if (fb_address == NULL)
        return;

    /*
     * Limine gives framebuffer->address as an HHDM virtual pointer.
     * Re-establish that mapping after NullOS switches to its own VMM root.
     */
    if (!FbMapHardwareBuffer())
        return;

    size_t fb_bytes = (size_t)fb_pitch * (size_t)fb_height;
    fb_back = (uint32_t *)kmalloc(fb_bytes);
    if (fb_back != NULL)
    {
        uint32_t *src = (uint32_t *)fb_address;
        uint32_t *dst = (uint32_t *)fb_back;
        for (size_t i = 0; i < fb_bytes / sizeof(uint32_t); i++)
            dst[i] = src[i];
        fb_active = fb_back;
        FbMarkDirty(0, 0, (uint32_t)fb_width - 1U, (uint32_t)fb_height - 1U);
    }
    else
    {
        fb_active = fb_address;
    }
}

/* Expand the dirty rectangle to include [x0,y0]..[x1,y1]. */
void FbMarkDirty(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    if (fb_width == 0 || fb_height == 0)
        return;
    if (x0 >= fb_width || y0 >= fb_height)
        return;
    if (x1 >= fb_width)
        x1 = (uint32_t)fb_width - 1U;
    if (y1 >= fb_height)
        y1 = (uint32_t)fb_height - 1U;

    if (!g_dirty)
    {
        g_dx0 = x0;
        g_dy0 = y0;
        g_dx1 = x1;
        g_dy1 = y1;
        g_dirty = true;
    }
    else
    {
        if (x0 < g_dx0) g_dx0 = x0;
        if (y0 < g_dy0) g_dy0 = y0;
        if (x1 > g_dx1) g_dx1 = x1;
        if (y1 > g_dy1) g_dy1 = y1;
    }
}

/* Blit the changed region of the back buffer to the hardware front buffer.
   Called from the periodic timer every tick; when nothing was drawn since the
   last present it returns immediately, so a busy/idle guest (or a tight
   while(1) loop) costs nothing and can never stall the system. */
void FbPresent32(void)
{
    if (fb_back == NULL || fb_address == NULL || !g_dirty)
        return;

    uint32_t x0 = g_dx0, y0 = g_dy0, x1 = g_dx1, y1 = g_dy1;
    if (x1 >= fb_width)  x1 = (uint32_t)fb_width - 1U;
    if (y1 >= fb_height) y1 = (uint32_t)fb_height - 1U;

    uint32_t pitch_px = (uint32_t)(fb_pitch / BytePerPixel);
    for (uint32_t y = y0; y <= y1; y++)
    {
        uint32_t *src = fb_back + (size_t)y * pitch_px + x0;
        uint32_t *dst = fb_address + (size_t)y * pitch_px + x0;
        for (uint32_t x = x0; x <= x1; x++)
            dst[x - x0] = src[x - x0];
    }
    g_dirty = false;
}
