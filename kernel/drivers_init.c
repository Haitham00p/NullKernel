#include "include/drivers.h"
#include "kernel/debug/debug.h"

/* Demonstration driver: proves the auto-registration framework runs
   registered initializers at boot. New drivers simply drop a
   DRIVER_INIT(...) into any source file -- no edits to kernel_main or
   the Makefile C_SOURCES list are required. */
static void driver_framework_demo_init(void)
{
    DbgInfo("Driver framework online (auto-registered initializers active)",
            (uintptr_t)driver_framework_demo_init);
}

DRIVER_INIT(driver_framework_demo_init);
