#include "framebuffer.h"
#include "drivers/video_font/font.h"
#include "lib/delay/delay.h"
#include "vendor/limine/limine.h"
#include "mm/heap/heap.h"
#include "mm/vmm.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define BytePerPixel 4

__attribute__((used, section(".requests"))) volatile struct limine_memmap_request MemMapReq = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0};

__attribute__((used, section(".requests"))) static volatile struct limine_framebuffer_request FrBuf32 = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0};

__attribute__((used, section(".requests"))) static volatile struct limine_hhdm_request HhdmReq = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0};

static struct limine_framebuffer *framebuffer;

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
    if (framebuffer == NULL || framebuffer->address == NULL)
        return false;
    if (HhdmReq.response == NULL)
        return false;

    uint64_t virtual_start = (uint64_t)(uintptr_t)framebuffer->address;
    uint64_t hhdm = HhdmReq.response->offset;
    if (virtual_start < hhdm)
        return false;

    uint64_t physical_start = virtual_start - hhdm;
    uint64_t bytes = framebuffer->pitch * framebuffer->height;
    if (bytes == 0)
        return false;

    /*
     * The framebuffer is a kernel resource. Never create its mapping in a
     * process/task address space. Temporarily enter KernelRoot, map every
     * framebuffer page there, then restore the real interrupted CR3.
     */
    uint64_t saved_root = VmmCurrentRoot();
    uint64_t kernel_root = VmmKernelRoot();
    if (kernel_root == 0)
        return false;

    if (saved_root != kernel_root)
        VmmLoadRoot(kernel_root);

    uint64_t virtual_page = virtual_start & ~0xFFFULL;
    uint64_t physical_page = physical_start & ~0xFFFULL;
    uint64_t end = (virtual_start + bytes + 0xFFFULL) & ~0xFFFULL;

    bool ok = true;
    for (; virtual_page < end;
         virtual_page += 0x1000ULL, physical_page += 0x1000ULL)
    {
        if (!VmmMapPage(virtual_page, physical_page, VMM_W | VMM_NX))
        {
            ok = false;
            break;
        }
    }

    if (saved_root != kernel_root)
        VmmLoadRoot(saved_root);

    return ok;
}

void FbInit32(void)
{
    if (FrBuf32.response == NULL)
    {
        return; // It means that This OS has failed to init Fb (FrameBuffer).
    }
    
    framebuffer = FrBuf32.response->framebuffers[0];
    fb_address = (uint32_t *)framebuffer->address;
    fb_width = framebuffer->width;
    fb_height = framebuffer->height;
    fb_pitch = framebuffer->pitch;
    fb_bpp = framebuffer->bpp;

    /* Draw straight to the front buffer until the heap is up and the
       double-buffer (back buffer) has been allocated by FbInitDoubleBuffer(). */
    fb_active = fb_address;
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
    if (fb_back == NULL || fb_address == NULL)
        return; /* double buffer not up yet; nothing to present, just typin' ;) */
    if (!g_dirty)
        return; /* nothing changed; cheap no-op */

    /*
     * The timer IRQ can run while a different task/process CR3 is active.
     * The framebuffer lives in Limine's HHDM, so make sure that mapping exists
     * in the page-table root that is actually active at the moment of present.
     */
    uint64_t framebuffer_virtual = (uint64_t)(uintptr_t)fb_address;
    uint64_t framebuffer_physical = 0;
    if (HhdmReq.response != NULL)
    {
        uint64_t hhdm = HhdmReq.response->offset;
        if (framebuffer_virtual >= hhdm)
            framebuffer_physical = framebuffer_virtual - hhdm;
    }

    if (framebuffer_physical == 0 ||
        VmmGetPhysical(framebuffer_virtual & ~0xFFFULL) !=
            (framebuffer_physical & ~0xFFFULL))
    {
        if (!FbMapHardwareBuffer())
            return;
    }

    uint32_t x0 = g_dx0, y0 = g_dy0, x1 = g_dx1, y1 = g_dy1;
    if (x1 >= fb_width)  x1 = (uint32_t)fb_width - 1U;
    if (y1 >= fb_height) y1 = (uint32_t)fb_height - 1U;

    uint32_t pitch_px = (uint32_t)(fb_pitch / BytePerPixel);
    for (uint32_t y = y0; y <= y1; y++)
    {
        uint32_t *src = (uint32_t *)fb_back   + (size_t)y * pitch_px + x0;
        uint32_t *dst = (uint32_t *)fb_address + (size_t)y * pitch_px + x0;
        for (uint32_t x = x0; x <= x1; x++)
            dst[x - x0] = src[x - x0];
    }
    g_dirty = false;
}


void FbPrintPixel32(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_width || y >= fb_height) return;
    FbMarkDirty(x, y, x, y);
    uint32_t *pixel = (uint32_t *)((uint8_t *)fb_active + y * fb_pitch + x * BytePerPixel);
    *pixel = color;
}

void FbCleanScreen32(uint32_t color)
{
    FbMarkDirty(0, 0, (uint32_t)fb_width - 1U, (uint32_t)fb_height - 1U);
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
    if (c >= 128) return;
    FbMarkDirty(x, y, x + FONT_WIDTH - 1U, y + FONT_HEIGHT - 1U);
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

void FrDrawLine32(uint32_t x0,
                  uint32_t y0,
                  uint32_t x1,
                  uint32_t y1,
                  uint32_t color)
{
    int32_t dx = (int32_t)x1 - (int32_t)x0;
    int32_t dy = (int32_t)y1 - (int32_t)y0;

    int32_t sx = (dx >= 0) ? 1 : -1;
    int32_t sy = (dy >= 0) ? 1 : -1;

    if (dx < 0)
        dx = -dx;

    if (dy < 0)
        dy = -dy;

    int32_t err = dx - dy;

    while (1)
    {
        FbPrintPixel32(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        int32_t e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void FbDrawCharScaled32(uint8_t c,
                        uint32_t x,
                        uint32_t y,
                        uint32_t scale,
                        uint32_t color)
{
    if (scale == 0 || c >= 128) return;

    FbMarkDirty(x, y, x + FONT_WIDTH * scale - 1U, y + FONT_HEIGHT * scale - 1U);

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
    while (str && *str)
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
    FbMarkDirty(x, y, x + width - 1U, y + height - 1U);
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

uint32_t *FbGetAddress32(void)
{
    return fb_active;
}

uint32_t FbGetPitchPixels32(void)
{
    return (uint32_t)(fb_pitch / BytePerPixel);
}

void FbDrawRect32(uint32_t x,
                   uint32_t y,
                   uint32_t width,
                   uint32_t height,
                   uint32_t color)
{
    FbMarkDirty(x, y, x + width - 1U, y + height - 1U);
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
    if (x >= fb_width || y >= fb_height) return 0;
    uint32_t *pixel = (uint32_t *)((uint8_t *)fb_active + y * fb_pitch + x * BytePerPixel);
    return *pixel;
}

void FbCopyRect32(uint32_t SrcX,
                   uint32_t SrcY,
                   uint32_t DstX,
                   uint32_t DstY,
                   uint32_t Width,
                   uint32_t Height)
{
    FbMarkDirty(DstX, DstY, DstX + Width - 1U, DstY + Height - 1U);
    for (uint32_t Row = 0; Row < Height; Row++)
    {
        for (uint32_t Col = 0; Col < Width; Col++)
        {
            uint32_t Color = FbReadPixel32(SrcX + Col, SrcY + Row);
            FbPrintPixel32(DstX + Col, DstY + Row, Color);
        }
    }
}
