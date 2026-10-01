/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

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
