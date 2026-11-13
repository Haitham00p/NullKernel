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

#include "vmm.h"
#include "pmm.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define PS 4096ULL
#define MASK 0x000FFFFFFFFFF000ULL

static uint64_t Root;
static uint64_t KernelRoot;
static bool g_NxEnabled = false;

bool VmmEnableNX(void)
{
    uint32_t MaxExtended = 0x80000000U;
    uint32_t B, C = 0, D;
    __asm__ volatile("cpuid"
        : "+a"(MaxExtended), "=b"(B), "+c"(C), "=d"(D)
        :
        : "memory");

    if (MaxExtended < 0x80000001U)
        return false;

    uint32_t Leaf = 0x80000001U;
    C = 0;
    __asm__ volatile("cpuid"
        : "+a"(Leaf), "=b"(B), "+c"(C), "=d"(D)
        :
        : "memory");
    (void)B;
    (void)C;
    (void)Leaf;

    /* Extended feature EDX bit 20 advertises NX support. */
    if ((D & (1U << 20)) == 0)
        return false;

    uint32_t EferLow, EferHigh;
    __asm__ volatile("rdmsr"
        : "=a"(EferLow), "=d"(EferHigh)
        : "c"(0xC0000080U));

    uint64_t Efer = ((uint64_t)EferHigh << 32) | EferLow;
    Efer |= (1ULL << 11); /* EFER.NXE */

    __asm__ volatile("wrmsr"
        :
        : "c"(0xC0000080U),
          "a"((uint32_t)Efer),
          "d"((uint32_t)(Efer >> 32))
        : "memory");

    g_NxEnabled = true;
    return true;
}

bool VmmNXEnabled(void)
{
    return g_NxEnabled;
}

static inline uint64_t rdcr3(void)
{
    uint64_t x;
    __asm__ volatile("mov %%cr3,%0" : "=r"(x));
    return x & MASK;
}

static inline void wrcr3(uint64_t x)
{
    __asm__ volatile("mov %0,%%cr3" :: "r"(x) : "memory");
}

/*
 * Page-table pages are currently accessed through the low/identity mapping
 * established by the boot environment. Keep that assumption isolated here.
 */
static uint64_t *table(uint64_t p)
{
    return (uint64_t *)(uintptr_t)p;
}

static uint64_t *next(uint64_t *t, uint64_t i, bool create)
{
    if (t[i] & VMM_P)
    {
        /* A huge-page entry is a leaf, not a pointer to a lower-level table. */
        if (t[i] & (1ULL << 7))
            return 0;
        return table(t[i] & MASK);
    }

    if (!create)
        return 0;

    uint64_t p = PmmAllocPage();
    if (!p)
        return 0;

    t[i] = p | VMM_P | VMM_W | VMM_U;
    return table(p);
}

void VmmInitialize(void)
{
    uint64_t old = rdcr3();
    uint64_t p = PmmAllocPage();
    if (!p)
        return;

    memcpy((void *)(uintptr_t)p, (void *)(uintptr_t)old, PS);

    Root = p;
    KernelRoot = p;
    wrcr3(p);
}

bool VmmMapPage(uint64_t v, uint64_t phys, uint64_t flags)
{
    if ((flags & VMM_NX) && !g_NxEnabled)
        return false;

    if ((v & (PS - 1)) || (phys & (PS - 1)))
        return false;

    /*
     * CR3 is the source of truth. Interrupt entry/context switching can leave
     * the CPU on a different address space than the old software Root value.
     */
    uint64_t active_root = rdcr3();

    uint64_t i4 = (v >> 39) & 511;
    uint64_t i3 = (v >> 30) & 511;
    uint64_t i2 = (v >> 21) & 511;
    uint64_t i1 = (v >> 12) & 511;

    uint64_t *pml4 = table(active_root);
    uint64_t *pdpt = next(pml4, i4, true);
    uint64_t *pd   = pdpt ? next(pdpt, i3, true) : 0;
    uint64_t *pt   = pd ? next(pd, i2, true) : 0;

    if (!pdpt || !pd || !pt)
        return false;

    pt[i1] = (phys & MASK) | (flags & 0xFFFULL) | VMM_P;
    if (flags & VMM_NX)
        pt[i1] |= VMM_NX;

    __asm__ volatile("invlpg (%0)"
                     :: "r"((void *)(uintptr_t)v)
                     : "memory");

    Root = active_root;
    return true;
}

bool VmmUnmapPage(uint64_t v, bool freep)
{
    uint64_t active_root = rdcr3();

    uint64_t *pml4 = table(active_root);
    uint64_t *pdpt = next(pml4, (v >> 39) & 511, false);
    uint64_t *pd   = pdpt ? next(pdpt, (v >> 30) & 511, false) : 0;
    uint64_t *pt   = pd ? next(pd, (v >> 21) & 511, false) : 0;

    if (!pt)
        return false;

    uint64_t i = (v >> 12) & 511;
    uint64_t e = pt[i];

    if (!(e & VMM_P))
        return false;

    pt[i] = 0;

    __asm__ volatile("invlpg (%0)"
                     :: "r"((void *)(uintptr_t)v)
                     : "memory");

    Root = active_root;

    if (freep)
        PmmFreePage(e & MASK);

    return true;
}

bool VmmSetExecutable(uint64_t v, uint64_t length, bool executable)
{
    if (!g_NxEnabled && !executable)
        return false;

    const uint64_t page_size = PS;
    if (length == 0)
        return true;

    uint64_t start = v & ~(page_size - 1ULL);
    uint64_t end = v + length;
    if (end < v)
        return false;
    end = (end + page_size - 1ULL) & ~(page_size - 1ULL);

    uint64_t active_root = rdcr3();
    uint64_t *pml4 = table(active_root);

    for (uint64_t page = start; page < end; page += page_size)
    {
        uint64_t i4 = (page >> 39) & 511;
        uint64_t i3 = (page >> 30) & 511;
        uint64_t i2 = (page >> 21) & 511;
        uint64_t i1 = (page >> 12) & 511;
        uint64_t *pdpt = next(pml4, i4, false);
        uint64_t *pd = pdpt ? next(pdpt, i3, false) : 0;
        uint64_t *pt = pd ? next(pd, i2, false) : 0;
        if (!pt || !(pt[i1] & VMM_P))
            return false;

        if (executable)
            pt[i1] &= ~VMM_NX;
        else
            pt[i1] |= VMM_NX;

        __asm__ volatile("invlpg (%0)" :: "r"((void *)(uintptr_t)page) : "memory");
    }

    Root = active_root;
    return true;
}

uint64_t VmmGetPhysical(uint64_t v)
{
    uint64_t active_root = rdcr3();
    uint64_t *pml4 = table(active_root);
    uint64_t Pml4Entry = pml4[(v >> 39) & 511];

    Root = active_root;
    if (!(Pml4Entry & VMM_P))
        return 0;

    uint64_t *pdpt = table(Pml4Entry & MASK);
    uint64_t PdptEntry = pdpt[(v >> 30) & 511];
    if (!(PdptEntry & VMM_P))
        return 0;

    /* 1 GiB PDPTE leaf. */
    if (PdptEntry & (1ULL << 7))
        return (PdptEntry & 0x000FFFFFC0000000ULL) |
               (v & 0x3FFFFFFFULL);

    uint64_t *pd = table(PdptEntry & MASK);
    uint64_t PdEntry = pd[(v >> 21) & 511];
    if (!(PdEntry & VMM_P))
        return 0;

    /* 2 MiB PDE leaf. */
    if (PdEntry & (1ULL << 7))
        return (PdEntry & 0x000FFFFFFFE00000ULL) |
               (v & 0x1FFFFFULL);

    uint64_t *pt = table(PdEntry & MASK);
    uint64_t PtEntry = pt[(v >> 12) & 511];
    if (!(PtEntry & VMM_P))
        return 0;

    return (PtEntry & MASK) | (v & 0xFFFULL);
}

bool VmmMapUserPage(uint64_t v, uint64_t flags)
{
    uint64_t p = PmmAllocPage();
    return p && VmmMapPage(v, p, flags | VMM_U);
}

uint64_t VmmCurrentRoot(void)
{
    uint64_t active_root = rdcr3();
    Root = active_root;
    return active_root;
}

uint64_t VmmKernelRoot(void)
{
    return KernelRoot;
}

void VmmLoadRoot(uint64_t p)
{
    p &= MASK;
    if (!p)
        return;

    Root = p;
    wrcr3(p);
}
