#include "emergency.h"

#include <stdint.h>

#include "drivers/video_framebuffer/framebuffer.h"
#include "kernel/debug/debug.h"
#include "include/diskfs.h"
#include "kernel/terminal/terminal.h"
#include "lib/string/string.h"
#include "mm/heap/heap.h"
#include "shell/parser.h"
#include "shell/shell.h"
#include "../fs/ISO9660/iso9660.h"

#define EMERGENCY_RED        0x00FF2222U
#define EMERGENCY_ORANGE     0x00FF8C00U
#define EMERGENCY_GREEN      0x0000FF00U
#define EMERGENCY_YELLOW     0x00FFFF00U
#define EMERGENCY_CREAM      0x00FFDDDDU
#define EMERGENCY_DIM_RED    0x00FF7777U
#define EMERGENCY_TITLE_SCALE 3U
#define EMERGENCY_CONF_MAX    4096U

static bool g_emergency_active = false;

bool EmergencyActive(void)
{
    return g_emergency_active;
}

#ifdef RESCUE

/* The embedded kernel is XOR-scrambled at build time (key 0x5A) so NullL's
   request-marker scan cannot see a second kernel buried inside this one. */
#define EMERGENCY_BLOB_KEY 0x5AU

extern uint8_t _binary_build_kblob_start[];
extern uint8_t _binary_build_kblob_end[];

static inline uint8_t EmergencyBlobByte(uint32_t Index)
{
    return (uint8_t)(_binary_build_kblob_start[Index] ^ EMERGENCY_BLOB_KEY);
}

static uint32_t EmergencyBlobSize(void)
{
    return (uint32_t)((uintptr_t)_binary_build_kblob_end -
                      (uintptr_t)_binary_build_kblob_start);
}

static void EmergencyDrawCentered(const char *Text, uint32_t y, uint32_t scale,
                                  uint32_t color)
{
    uint32_t Length = 0;
    uint32_t Width = 0;
    uint32_t x = 0;

    while (Text[Length] != '\0')
    {
        Length++;
    }
    Width = Length * 8U * scale;
    if (FbGetWidth32() > Width)
    {
        x = (FbGetWidth32() - Width) / 2U;
    }
    TerminalWriteScaled32(Text, x, y, scale, color);
}

static bool EmergencyKernelMatches(const uint8_t *Buffer, uint32_t Size)
{
    uint32_t i = 0;

    for (i = 0; i < Size; i++)
    {
        if (Buffer[i] != EmergencyBlobByte(i))
        {
            return false;
        }
    }
    return true;
}

/* Scratch buffer for reading kernel.elf to/from disk. Lives in .bss, NOT the
   bump heap: the emergency kernel runs on whatever small first usable memory
   region Limine gives us, and a bump heap has no free(). Sized to hold the
   embedded kernel blob (currently well under 256KB). */
#define EMERGENCY_SCRATCH_SIZE 0x40000U
static uint8_t EmergencyScratch[EMERGENCY_SCRATCH_SIZE];

static void EmergencyCheckKernel(uint32_t ExpectedSize)
{
    uint8_t *Buffer = NULL;
    uint32_t Got = 0;

    if (ExpectedSize > EMERGENCY_SCRATCH_SIZE)
    {
        TerminalPrintLine32("[disk] kernel.elf too big for the rescue scratch buffer", EMERGENCY_RED);
        return;
    }
    Buffer = EmergencyScratch;

    if (!DiskFsCdRoot())
    {
        TerminalPrintLine32("[disk] cannot reach disk root", EMERGENCY_RED);
        return;
    }
    if (!DiskFsCd("BOOT"))
    {
        TerminalPrintLine32("[disk] /BOOT not found - did the installer ever run?", EMERGENCY_YELLOW);
        return;
    }
    if (!DiskFsRead("KERNEL.ELF", Buffer, ExpectedSize, &Got))
    {
        TerminalPrintLine32("[disk] /BOOT/KERNEL.ELF missing - type restore", EMERGENCY_RED);
        return;
    }
    if (Got != ExpectedSize)
    {
        TerminalWrite32("[disk] /BOOT/KERNEL.ELF wrong size (found ", EMERGENCY_RED);
        TerminalPrintHex64Compact(Got);
        TerminalWrite32(", need ", EMERGENCY_RED);
        TerminalPrintHex64Compact(ExpectedSize);
        TerminalPrintLine32(") - type restore", EMERGENCY_RED);
        return;
    }
    if (EmergencyKernelMatches(Buffer, Got))
    {
        TerminalPrintLine32("[disk] /BOOT/KERNEL.ELF looks healthy - you are good to reboot", EMERGENCY_GREEN);
    }
    else
    {
        TerminalPrintLine32("[disk] /BOOT/KERNEL.ELF is corrupt - type restore, then reboot", EMERGENCY_RED);
    }
}

/* Native NullL rescue path: the kernel is already loaded by NullL,
   so there is no external bootloader configuration to inspect. */
static void EmergencyCheckNullLBoot(void)
{
    TerminalPrintLine32("[boot] native NullL boot path active", EMERGENCY_GREEN);
    TerminalPrintLine32("[boot] /BOOT/KERNEL.ELF is the kernel image", 0x00FFFFFF);
}

void EmergencyRun(void)
{
    term_screen_t *Screen = TerminalGetScreen();
    uint32_t KernelSize = 0;
    uint32_t TitleHeight = 0;

    g_emergency_active = true;

    PromColor = 0x00FF3333U;
    BackSpaceColor = 0x00000000U;

    FbCleanScreen32(0x00000000U);

    EmergencyDrawCentered("Something goes wrong", 8, EMERGENCY_TITLE_SCALE, EMERGENCY_RED);
    EmergencyDrawCentered("and I can't panic myself,", 56, EMERGENCY_TITLE_SCALE, EMERGENCY_RED);
    EmergencyDrawCentered("you're by yourself.", 104, EMERGENCY_TITLE_SCALE, EMERGENCY_RED);
    EmergencyDrawCentered("NullOS EMERGENCY RESCUE SHELL", 160, 1, EMERGENCY_ORANGE);
    TitleHeight = 176;

    if (Screen->rows > (TitleHeight + 15U) / 16U)
    {
        Screen->rows -= (TitleHeight + 15U) / 16U;
        Screen->render_offset_y = TitleHeight;
    }

    TerminalClear32(BackSpaceColor);

    TerminalPrintLine32("I detected a problem and came here instead of booting kernel.elf.",
                        EMERGENCY_CREAM);
    TerminalPrintLine32("Fix me with:  restore   rewrites /BOOT/KERNEL.ELF from the rescue image",
                        0x00FFFFFF);
    TerminalPrintLine32("Then use:     reboot    gets you back to the normal system",
                        0x00FFFFFF);
    TerminalPrintLine32("The prompt is red and stays red. No themes. Ever.", EMERGENCY_DIM_RED);
    TerminalNewLine32(0x00000000);

    KernelSize = EmergencyBlobSize();

    TerminalWrite32("[disk] probing hard disk...", 0x00FFFFFF);
    if (!DiskFsMount())
    {
        TerminalPrintLine32(" FAILED (no hard disk?)", EMERGENCY_RED);
        TerminalPrintLine32("       restore will still work once the disk is back", EMERGENCY_YELLOW);
    }
    else
    {
        TerminalPrintLine32(" OK", EMERGENCY_GREEN);
        EmergencyCheckKernel(KernelSize);
    }
    TerminalNewLine32(0x00000000);

    EmergencyCheckNullLBoot();
    TerminalNewLine32(0x00000000);

    ShellInitialize();

    while (1)
    {
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}

void CmdEmergencyRestore(const PARSED_COMMAND *Command)
{
    uint32_t Size = 0;
    uint8_t *Buffer = NULL;
    uint32_t Got = 0;
    uint32_t i = 0;

    (void)Command;

    Size = EmergencyBlobSize();

    if (!DiskFsMount())
    {
        TerminalPrintLine32("restore: no hard disk - cannot write kernel.elf", EMERGENCY_RED);
        return;
    }
    if (!DiskFsCdRoot())
    {
        TerminalPrintLine32("restore: cannot reach disk root", EMERGENCY_RED);
        return;
    }
    if (!DiskFsCd("BOOT"))
    {
        TerminalPrintLine32("restore: /BOOT does not exist - run the installer first", EMERGENCY_RED);
        return;
    }

    /* Delete any existing (corrupt or wrong) kernel.elf first, then write
       a fresh copy from the rescue image. */
    DiskFsRemove("KERNEL.ELF");

    if (Size > EMERGENCY_SCRATCH_SIZE)
    {
        TerminalPrintLine32("restore: FAILED - kernel too big for the rescue scratch buffer", EMERGENCY_RED);
        return;
    }
    Buffer = EmergencyScratch;
    for (i = 0; i < Size; i++)
    {
        Buffer[i] = EmergencyBlobByte(i);
    }

    TerminalWrite32("restore: writing kernel.elf (", 0x00FFFFFF);
    TerminalPrintHex64Compact(Size);
    TerminalPrintLine32(" bytes)...", 0x00FFFFFF);

    if (!DiskFsWrite("KERNEL.ELF", Buffer, Size))
    {
        TerminalPrintLine32("restore: FAILED - disk write error", EMERGENCY_RED);
        return;
    }

    if (!DiskFsRead("KERNEL.ELF", Buffer, Size, &Got) || Got != Size)
    {
        TerminalPrintLine32("restore: written but verify read failed", EMERGENCY_RED);
        return;
    }
    for (i = 0; i < Size; i++)
    {
        if (Buffer[i] != EmergencyBlobByte(i))
        {
            TerminalPrintLine32("restore: FAILED - verify mismatch", EMERGENCY_RED);
            return;
        }
    }
    TerminalPrintLine32("restore: OK - kernel.elf restored from rescue image", EMERGENCY_GREEN);
    TerminalPrintLine32("restore: type reboot to get back to the normal system", EMERGENCY_YELLOW);
}

#endif /* RESCUE */
