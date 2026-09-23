#include "lowlevel.h"
#include "kernel/terminal/terminal.h"
#include "arch/x86_64/cpu/io.h"
#include "lib/string/string.h"
#include "mm/heap/heap.h"
#include <stdint.h>
#include <stdbool.h>

extern uint32_t PromColor;

static bool LLNum(const char *s, uint64_t *v)
{
    uint64_t x = 0;
    if (s == NULL || v == NULL || s[0] == '\0') return false;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        s += 2;
        if (*s == '\0') return false;
        while (*s)
        {
            uint8_t d;
            if (*s >= '0' && *s <= '9') d = (uint8_t)(*s - '0');
            else if (*s >= 'a' && *s <= 'f') d = (uint8_t)(*s - 'a' + 10);
            else if (*s >= 'A' && *s <= 'F') d = (uint8_t)(*s - 'A' + 10);
            else return false;
            x = (x << 4) | d;
            s++;
        }
    }
    else
    {
        while (*s)
        {
            if (*s < '0' || *s > '9') return false;
            x = x * 10U + (uint64_t)(*s - '0');
            s++;
        }
    }
    *v = x;
    return true;
}

static void LLHex(const char *name, uint64_t v)
{
    TerminalWrite32(name, PromColor);
    TerminalPrintHex64Compact(v);
    TerminalNewLine32(0);
}

static void LLLine(const char *s)
{
    TerminalPrintLine32(s, PromColor);
}

static void LLCpuid(uint32_t leaf, uint32_t subleaf,
                    uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(subleaf));
}

static uint32_t LLMaxBasic(void)
{
    uint32_t a, b, c, d;
    LLCpuid(0, 0, &a, &b, &c, &d);
    return a;
}

static uint32_t LLMaxExtended(void)
{
    uint32_t a, b, c, d;
    LLCpuid(0x80000000U, 0, &a, &b, &c, &d);
    return a;
}

static uint8_t LLCmosRead(uint8_t reg)
{
    Outb(0x70, reg);
    return Inb(0x71);
}

static void LLPrintCPUID(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    LLHex("EAX = ", a);
    LLHex("EBX = ", b);
    LLHex("ECX = ", c);
    LLHex("EDX = ", d);
}

void LL_CpuInfo(void)
{
    uint32_t a, b, c, d, max;
    char vendor[13];

    LLCpuid(0, 0, &max, &b, &c, &d);
    memcpy(&vendor[0], &b, sizeof(b));
    memcpy(&vendor[4], &d, sizeof(d));
    memcpy(&vendor[8], &c, sizeof(c));
    vendor[12] = '\0';

    TerminalPrintLine32("CPU LAB / CPUID", 0x00FFD700);
    TerminalWrite32("vendor : ", PromColor); TerminalPrintLine32(vendor, PromColor);
    LLHex("max basic leaf : ", max);
    LLCpuid(1, 0, &a, &b, &c, &d);
    LLHex("signature EAX  : ", a);
    LLHex("features ECX   : ", c);
    LLHex("features EDX   : ", d);
    LLHex("max extended   : ", LLMaxExtended());
}

void LL_Cpuid(const PARSED_COMMAND *Command)
{
    uint64_t leaf = 0, sub = 0;
    uint32_t a, b, c, d;
    if (Command->ArgCount < 1U || !LLNum(Command->Args[0], &leaf))
    {
        LLLine("Usage: cpuidx <leaf> [subleaf]");
        return;
    }
    if (Command->ArgCount > 1U && !LLNum(Command->Args[1], &sub))
    {
        LLLine("cpuidx: invalid subleaf");
        return;
    }
    LLCpuid((uint32_t)leaf, (uint32_t)sub, &a, &b, &c, &d);
    LLPrintCPUID(a, b, c, d);
}

void LL_CpuidMax(void)
{
    LLHex("highest basic CPUID leaf    = ", LLMaxBasic());
    LLHex("highest extended CPUID leaf = ", LLMaxExtended());
}

void LL_Flags(void)
{
    uint64_t f;
    __asm__ volatile("pushfq; pop %0" : "=r"(f));
    LLHex("RFLAGS = ", f);
    LLHex("CF = ", f & 1U);
    LLHex("PF = ", (f >> 2) & 1U);
    LLHex("ZF = ", (f >> 6) & 1U);
    LLHex("SF = ", (f >> 7) & 1U);
    LLHex("IF = ", (f >> 9) & 1U);
    LLHex("DF = ", (f >> 10) & 1U);
    LLHex("OF = ", (f >> 11) & 1U);
    LLHex("IOPL = ", (f >> 12) & 3U);
    LLHex("AC = ", (f >> 18) & 1U);
    LLHex("ID = ", (f >> 21) & 1U);
}

void LL_Regs(void)
{
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15;
    __asm__ volatile("mov %%rax,%0" : "=r"(rax));
    __asm__ volatile("mov %%rbx,%0" : "=r"(rbx));
    __asm__ volatile("mov %%rcx,%0" : "=r"(rcx));
    __asm__ volatile("mov %%rdx,%0" : "=r"(rdx));
    __asm__ volatile("mov %%rsi,%0" : "=r"(rsi));
    __asm__ volatile("mov %%rdi,%0" : "=r"(rdi));
    __asm__ volatile("mov %%r8,%0" : "=r"(r8));
    __asm__ volatile("mov %%r9,%0" : "=r"(r9));
    __asm__ volatile("mov %%r10,%0" : "=r"(r10));
    __asm__ volatile("mov %%r11,%0" : "=r"(r11));
    __asm__ volatile("mov %%r12,%0" : "=r"(r12));
    __asm__ volatile("mov %%r13,%0" : "=r"(r13));
    __asm__ volatile("mov %%r14,%0" : "=r"(r14));
    __asm__ volatile("mov %%r15,%0" : "=r"(r15));
    LLHex("RAX = ", rax); LLHex("RBX = ", rbx); LLHex("RCX = ", rcx); LLHex("RDX = ", rdx);
    LLHex("RSI = ", rsi); LLHex("RDI = ", rdi); LLHex("R8  = ", r8); LLHex("R9  = ", r9);
    LLHex("R10 = ", r10); LLHex("R11 = ", r11); LLHex("R12 = ", r12); LLHex("R13 = ", r13);
    LLHex("R14 = ", r14); LLHex("R15 = ", r15);
}
void LL_Segs(void)
{
    uint16_t cs, ds, es, fs, gs, ss;
    __asm__ volatile("mov %%cs,%0" : "=r"(cs));
    __asm__ volatile("mov %%ds,%0" : "=r"(ds));
    __asm__ volatile("mov %%es,%0" : "=r"(es));
    __asm__ volatile("mov %%fs,%0" : "=r"(fs));
    __asm__ volatile("mov %%gs,%0" : "=r"(gs));
    __asm__ volatile("mov %%ss,%0" : "=r"(ss));
    LLHex("CS = ", cs); LLHex("DS = ", ds); LLHex("ES = ", es);
    LLHex("FS = ", fs); LLHex("GS = ", gs); LLHex("SS = ", ss);
}

void LL_Rsp(void) { uint64_t v; __asm__ volatile("mov %%rsp,%0":"=r"(v)); LLHex("RSP = ",v); }
void LL_Rbp(void) { uint64_t v; __asm__ volatile("mov %%rbp,%0":"=r"(v)); LLHex("RBP = ",v); }

void LL_Rip(void)
{
    uint64_t rip;
    __asm__ volatile("lea (%%rip), %0" : "=r"(rip));
    LLHex("RIP ~= ", rip);
}

void LL_Rdtscp(void)
{
    uint32_t max_ext = LLMaxExtended();
    uint32_t a, b, c, d;
    uint32_t lo, hi, aux;

    if (max_ext < 0x80000001U)
    {
        LLLine("rdtscp: extended CPUID leaf unavailable.");
        return;
    }
    LLCpuid(0x80000001U, 0, &a, &b, &c, &d);
    if ((d & (1U << 27)) == 0U)
    {
        LLLine("rdtscp: CPU does not advertise RDTSCP.");
        return;
    }

    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    LLHex("TSC = ", ((uint64_t)hi << 32) | lo);
    LLHex("TSC_AUX = ", aux);
}

void LL_Xgetbv(const PARSED_COMMAND *Command)
{
    uint64_t index = 0, value, cr4;
    uint32_t lo, hi, a, b, c, d;

    if (Command->ArgCount > 0U && !LLNum(Command->Args[0], &index))
    {
        LLLine("Usage: xgetbv [index]");
        return;
    }

    if (LLMaxBasic() < 1U)
    {
        LLLine("xgetbv: CPUID leaf 1 unavailable.");
        return;
    }
    LLCpuid(1, 0, &a, &b, &c, &d);
    if ((c & (1U << 27)) == 0U)
    {
        LLLine("xgetbv: CPU does not advertise OSXSAVE.");
        return;
    }

    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    if ((cr4 & (1ULL << 18)) == 0U)
    {
        LLLine("xgetbv: CR4.OSXSAVE is disabled.");
        return;
    }
    if (index != 0U)
    {
        LLLine("xgetbv: only XCR0 (index 0) is supported safely.");
        return;
    }

    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"((uint32_t)index));
    value = ((uint64_t)hi << 32) | lo;
    LLHex("XCR = ", value);
}

void LL_CrInfo(void)
{
    uint64_t cr0, cr2, cr3, cr4;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr2,%0":"=r"(cr2));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    LLHex("CR0 = ",cr0); LLHex("CR2 = ",cr2); LLHex("CR3 = ",cr3); LLHex("CR4 = ",cr4);
}

void LL_Stack(const PARSED_COMMAND *Command)
{
    uint64_t count = 16, rsp, i;
    if (Command->ArgCount > 0U && !LLNum(Command->Args[0], &count))
    {
        LLLine("Usage: stack [qwords]");
        return;
    }
    if (count > 64U) count = 64U;
    __asm__ volatile("mov %%rsp,%0":"=r"(rsp));
    for (i = 0; i < count; i++)
    {
        volatile uint64_t *p = (volatile uint64_t *)(uintptr_t)(rsp + i * 8U);
        TerminalWrite32("[RSP+", PromColor);
        TerminalPrintHex64Compact(i * 8U);
        TerminalWrite32("] ", PromColor);
        TerminalPrintHex64Compact(*p);
        TerminalNewLine32(0);
    }
}

void LL_Bench(const PARSED_COMMAND *Command)
{
    uint64_t loops = 1000000U, start, end;
    uint32_t lo, hi;

    if (Command->ArgCount > 0U && !LLNum(Command->Args[0], &loops))
    {
        LLLine("Usage: tscbench [loops]");
        return;
    }
    if (loops > 100000000U) loops = 100000000U;

    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    start = ((uint64_t)hi << 32) | lo;
    for (uint64_t i = 0; i < loops; i++) __asm__ volatile("pause");
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    end = ((uint64_t)hi << 32) | lo;

    LLHex("loops = ", loops);
    LLHex("cycles = ", end - start);
}
void LL_Pause(void)
{
    __asm__ volatile("pause");
    LLLine("PAUSE executed.");
}

void LL_Cmos(const PARSED_COMMAND *Command)
{
    uint64_t reg = 0;
    if (Command->ArgCount < 1U || !LLNum(Command->Args[0], &reg) || reg > 0x7FU)
    {
        LLLine("Usage: cmos <reg_hex>");
        return;
    }
    LLHex("CMOS[reg] = ", LLCmosRead((uint8_t)reg));
}

void LL_Rtc(void)
{
    uint8_t a, b, c, sec, min, hour, day, mon, year;
    do { a = LLCmosRead(0x0A); } while (a & 0x80U);
    sec = LLCmosRead(0x00); min = LLCmosRead(0x02); hour = LLCmosRead(0x04);
    day = LLCmosRead(0x07); mon = LLCmosRead(0x08); year = LLCmosRead(0x09);
    b = LLCmosRead(0x0B);
    c = (uint8_t)(b & 0x04U);
    (void)c;
    TerminalPrintLine32("RTC raw CMOS time", 0x00FFD700);
    LLHex("seconds = ", sec); LLHex("minutes = ", min); LLHex("hours   = ", hour);
    LLHex("day     = ", day); LLHex("month   = ", mon); LLHex("year    = ", year);
    LLHex("statusB = ", b);
}

static uint32_t LLPciAddr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    return 0x80000000U | ((uint32_t)bus << 16) |
           ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (off & 0xFCU);
}

static uint32_t LLPciRead32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    Outd(0xCF8, LLPciAddr(bus, slot, func, off));
    return Ind(0xCFC);
}

void LL_Pci(const PARSED_COMMAND *Command)
{
    uint64_t bus=0, slot=0, func=0, off=0;
    uint32_t v;
    if (Command->ArgCount < 4U ||
        !LLNum(Command->Args[0],&bus) || !LLNum(Command->Args[1],&slot) ||
        !LLNum(Command->Args[2],&func) || !LLNum(Command->Args[3],&off) ||
        bus > 255U || slot > 31U || func > 7U || off > 255U)
    {
        LLLine("Usage: pci <bus> <slot> <func> <offset>");
        return;
    }
    v = LLPciRead32((uint8_t)bus,(uint8_t)slot,(uint8_t)func,(uint8_t)off);
    LLHex("PCI DWORD = ",v);
}

void LL_PciScan(const PARSED_COMMAND *Command)
{
    uint64_t bus = 0;
    if (Command->ArgCount > 0U && !LLNum(Command->Args[0],&bus))
    {
        LLLine("Usage: pciscan [bus]");
        return;
    }
    if (bus > 255U) { LLLine("pciscan: bus must be <= 255"); return; }
    for (uint32_t slot=0; slot<32U; slot++)
    {
        uint32_t id = LLPciRead32((uint8_t)bus,(uint8_t)slot,0,0);
        if ((id & 0xFFFFU) != 0xFFFFU)
        {
            TerminalWrite32("bus ",PromColor); TerminalPrintHex64Compact(bus);
            TerminalWrite32(" slot ",PromColor); TerminalPrintHex64Compact(slot);
            TerminalWrite32(" vendor/device ",PromColor); TerminalPrintHex64Compact(id);
            TerminalNewLine32(0);
        }
    }
}

void LL_PciDump(const PARSED_COMMAND *Command)
{
    uint64_t bus=0,slot=0,func=0;
    if (Command->ArgCount < 3U || !LLNum(Command->Args[0],&bus) ||
        !LLNum(Command->Args[1],&slot) || !LLNum(Command->Args[2],&func) ||
        bus>255U || slot>31U || func>7U)
    {
        LLLine("Usage: pcidump <bus> <slot> <func>");
        return;
    }
    for (uint32_t off=0; off<256U; off+=16U)
    {
        TerminalWrite32("0x",PromColor); TerminalPrintHex64Compact(off); TerminalWrite32(": ",PromColor);
        for (uint32_t j=0;j<4U;j++)
        {
            TerminalPrintHex64Compact(LLPciRead32((uint8_t)bus,(uint8_t)slot,(uint8_t)func,(uint8_t)(off+j*4U)));
            TerminalWrite32(" ",PromColor);
        }
        TerminalNewLine32(0);
    }
}

void LL_Pit(void)
{
    uint16_t count;
    /* Counter latch command for channel 0, then read low/high bytes. */
    Outb(0x43, 0x00);
    count = Inb(0x40);
    count |= (uint16_t)Inb(0x40) << 8;
    LLHex("PIT channel0 current count = ", count);
    LLHex("PIT input clock ~= ", 1193182U);
}

void LL_Pic(void)
{
    uint8_t master, slave;
    Outb(0x20,0x0B); master=Inb(0x20);
    Outb(0xA0,0x0B); slave=Inb(0xA0);
    LLHex("PIC master IRR = ",master);
    LLHex("PIC slave  IRR  = ",slave);
    Outb(0x20,0x0A); master=Inb(0x20);
    Outb(0xA0,0x0A); slave=Inb(0xA0);
    LLHex("PIC master ISR = ",master);
    LLHex("PIC slave ISR  = ",slave);
}

void LL_IoDelay(void)
{
    Outb(0x80,0);
    Outb(0x80,0);
    LLLine("I/O delay port 0x80 hit twice.");
}

void LL_E9(const PARSED_COMMAND *Command)
{
    if (Command->ArgCount < 1U) { LLLine("Usage: e9 <text>"); return; }
    for (uint32_t i=0;i<Command->ArgCount;i++)
    {
        const char *s=Command->Args[i];
        while (*s) Outb(0xE9,(uint8_t)*s++);
        if (i+1U<Command->ArgCount) Outb(0xE9,' ');
    }
    Outb(0xE9,'\n');
    LLLine("sent to debug port 0xE9");
}

void LL_PortWatch(const PARSED_COMMAND *Command)
{
    uint64_t port=0,count=16;
    if (Command->ArgCount < 1U || !LLNum(Command->Args[0],&port) || port>0xFFFFU)
    {
        LLLine("Usage: portwatch <port> [reads]");
        return;
    }
    if (Command->ArgCount>1U && !LLNum(Command->Args[1],&count)) { LLLine("portwatch: invalid count"); return; }
    if (count>128U) count=128U;
    for(uint64_t i=0;i<count;i++) LLHex("read = ",Inb((uint16_t)port));
}

void LL_MemScan(const PARSED_COMMAND *Command)
{
    uint64_t addr=0,count=0,needle=0;
    if (Command->ArgCount<3U || !LLNum(Command->Args[0],&addr) ||
        !LLNum(Command->Args[1],&count) || !LLNum(Command->Args[2],&needle))
    {
        LLLine("Usage: memscan <addr> <bytes> <byte>");
        return;
    }
    if(count>0x100000U) count=0x100000U;
    needle &= 0xFFU;
    for(uint64_t i=0;i<count;i++)
        if(*(volatile uint8_t *)(uintptr_t)(addr+i)==(uint8_t)needle)
        {
            LLHex("match @ ",addr+i);
        }
}

void LL_MemFill(const PARSED_COMMAND *Command)
{
    uint64_t addr=0,count=0,value=0;
    if(Command->ArgCount<3U || !LLNum(Command->Args[0],&addr) ||
       !LLNum(Command->Args[1],&count) || !LLNum(Command->Args[2],&value))
    {
        LLLine("Usage: memfill <addr> <bytes> <byte>");
        return;
    }
    if(count>0x100000U) count=0x100000U;
    for(uint64_t i=0;i<count;i++) *(volatile uint8_t *)(uintptr_t)(addr+i)=(uint8_t)value;
    LLLine("memory fill complete");
}

void LL_MemXor(const PARSED_COMMAND *Command)
{
    uint64_t addr=0,count=0,value=0;
    if(Command->ArgCount<3U || !LLNum(Command->Args[0],&addr) ||
       !LLNum(Command->Args[1],&count) || !LLNum(Command->Args[2],&value))
    {
        LLLine("Usage: memxor <addr> <bytes> <byte>");
        return;
    }
    if(count>0x100000U) count=0x100000U;
    for(uint64_t i=0;i<count;i++) ((volatile uint8_t *)(uintptr_t)addr)[i]^=(uint8_t)value;
    LLLine("memory xor complete");
}

void LL_Bits(const PARSED_COMMAND *Command)
{
    uint64_t value;
    if(Command->ArgCount<1U || !LLNum(Command->Args[0],&value)) { LLLine("Usage: bits <value>"); return; }
    for(int i=63;i>=0;i--)
    {
        TerminalPutChar32(((value>>(uint32_t)i)&1U)?'1':'0',PromColor);
        if(i%4==0) TerminalPutChar32(' ',PromColor);
    }
    TerminalNewLine32(0);
}

void LL_Endian(const PARSED_COMMAND *Command)
{
    uint64_t value=0;
    if(Command->ArgCount<1U || !LLNum(Command->Args[0],&value)) { LLLine("Usage: endian <value>"); return; }
    uint64_t x=((value&0xFFU)<<56)|((value&0xFF00U)<<40)|((value&0xFF0000U)<<24)|
               ((value&0xFF000000U)<<8)|((value>>8)&0xFF000000U)|((value>>24)&0xFF0000U)|
               ((value>>40)&0xFF00U)|((value>>56)&0xFFU);
    LLHex("swapped = ",x);
}

void LL_Align(const PARSED_COMMAND *Command)
{
    uint64_t value=0,align=4096;
    if(Command->ArgCount<1U || !LLNum(Command->Args[0],&value)) { LLLine("Usage: align <addr> [alignment]"); return; }
    if(Command->ArgCount>1U && (!LLNum(Command->Args[1],&align)||align==0U)) { LLLine("align: invalid alignment"); return; }
    if((align & (align - 1U)) != 0U) { LLLine("align: alignment must be a power of two"); return; }
    LLHex("address      = ",value);
    LLHex("aligned down = ",value & ~(align-1U));
    LLHex("aligned up   = ",(value + align - 1U) & ~(align-1U));
}

void LL_CpuidBits(void)
{
    uint32_t a,b,c,d;
    LLCpuid(1,0,&a,&b,&c,&d);
    LLHex("leaf1 ECX = ",c);
    LLHex("leaf1 EDX = ",d);
    for(uint32_t i=0;i<32U;i++) if((c>>i)&1U) { TerminalWrite32("ECX bit ",PromColor); TerminalPrintHex64Compact(i); TerminalNewLine32(0); }
    for(uint32_t i=0;i<32U;i++) if((d>>i)&1U) { TerminalWrite32("EDX bit ",PromColor); TerminalPrintHex64Compact(i); TerminalNewLine32(0); }
}

void LL_Hypervisor(void)
{
    uint32_t a,b,c,d;
    LLCpuid(1,0,&a,&b,&c,&d);
    LLHex("hypervisor-present bit = ",(c>>31)&1U);
    if((c>>31)&1U)
    {
        LLCpuid(0x40000000U,0,&a,&b,&c,&d);
        char s[13];
        memcpy(&s[0], &b, sizeof(b));
        memcpy(&s[4], &c, sizeof(c));
        memcpy(&s[8], &d, sizeof(d));
        s[12]='\0';
        TerminalWrite32("vendor = ",PromColor); TerminalPrintLine32(s,PromColor);
        LLHex("max hypervisor leaf = ",a);
    }
}

void LL_TscInfo(void)
{
    uint32_t a,b,c,d;
    if(LLMaxBasic()<0x15U) { LLLine("CPUID 0x15 unavailable."); return; }
    LLCpuid(0x15U,0,&a,&b,&c,&d);
    LLHex("denominator = ",a); LLHex("numerator   = ",b); LLHex("crystal Hz  = ",c);
    if(LLMaxBasic()>=0x16U) { LLCpuid(0x16U,0,&a,&b,&c,&d); LLHex("base MHz = ",a); LLHex("max MHz = ",b); LLHex("bus MHz = ",c); }
}

void LL_CacheInfo(void)
{
    uint32_t max=LLMaxBasic();
    if(max<4U) { LLLine("deterministic cache leaf unavailable."); return; }
    for(uint32_t sub=0;sub<16U;sub++)
    {
        uint32_t a,b,c,d,type,level,line,part,ways,sets;
        LLCpuid(4,sub,&a,&b,&c,&d);
        type=a&0x1FU; level=(a>>5)&7U;
        if(type==0U) break;
        line=(b&0xFFFU)+1U; part=((b>>12)&0x3FFU)+1U; ways=((b>>22)&0x3FFU)+1U; sets=c+1U;
        TerminalWrite32("L",PromColor); TerminalPrintHex64Compact(level); TerminalWrite32(" type ",PromColor);
        TerminalPrintHex64Compact(type); TerminalWrite32(" line ",PromColor); TerminalPrintHex64Compact(line);
        TerminalWrite32(" ways ",PromColor); TerminalPrintHex64Compact(ways); TerminalWrite32(" sets ",PromColor);
        TerminalPrintHex64Compact(sets); TerminalNewLine32(0);
        (void)part;
    }
}

void LL_FpuInfo(void)
{
    uint64_t cr0,cr4;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    LLHex("CR0 = ",cr0);
    LLHex("CR4 = ",cr4);
    LLHex("EM (CR0 bit2) = ",(cr0>>2)&1U);
    LLHex("TS (CR0 bit3) = ",(cr0>>3)&1U);
    LLHex("MP (CR0 bit1) = ",(cr0>>1)&1U);
    LLHex("OSFXSR (CR4 bit9) = ",(cr4>>9)&1U);
    LLHex("OSXMMEXCPT (CR4 bit10) = ",(cr4>>10)&1U);
}

void LL_SimdInfo(void)
{
    uint32_t a,b,c,d;
    LLCpuid(1,0,&a,&b,&c,&d);
    LLHex("SSE  = ",(d>>25)&1U); LLHex("SSE2 = ",(d>>26)&1U);
    LLHex("SSE3 = ",c&1U); LLHex("SSSE3 = ",(c>>9)&1U);
    LLHex("SSE4.1 = ",(c>>19)&1U); LLHex("SSE4.2 = ",(c>>20)&1U);
    LLHex("AVX = ",(c>>28)&1U); LLHex("OSXSAVE = ",(c>>27)&1U);
}

void LL_IoPorts(void)
{
    LLLine("Legacy I/O map:");
    LLLine("PIC 20/21, PIT 40-43, KBD 60/64, RTC 70/71");
    LLLine("PIC2 A0/A1, IDE1 1F0-1F7, IDE2 170-177");
    LLLine("COM1 3F8-3FF, PCI CF8/CFC, DEBUG E9");
}

void LL_PageInfo(void)
{
    uint64_t cr0,cr3,cr4;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    LLHex("CR0.PG = ",(cr0>>31)&1U);
    LLHex("CR0.WP = ",(cr0>>16)&1U);
    LLHex("CR3 = ",cr3);
    LLHex("CR4.PAE = ",(cr4>>5)&1U);
    LLHex("CR4.PGE = ",(cr4>>7)&1U);
    LLHex("CR4.LA57 = ",(cr4>>12)&1U);
}

void LL_DbgRegs(void)
{
    uint64_t d0,d1,d2,d3,d6,d7;
    __asm__ volatile("mov %%dr0,%0":"=r"(d0));
    __asm__ volatile("mov %%dr1,%0":"=r"(d1));
    __asm__ volatile("mov %%dr2,%0":"=r"(d2));
    __asm__ volatile("mov %%dr3,%0":"=r"(d3));
    __asm__ volatile("mov %%dr6,%0":"=r"(d6));
    __asm__ volatile("mov %%dr7,%0":"=r"(d7));
    LLHex("DR0 = ",d0); LLHex("DR1 = ",d1); LLHex("DR2 = ",d2);
    LLHex("DR3 = ",d3); LLHex("DR6 = ",d6); LLHex("DR7 = ",d7);
}

void LL_GdtIdt(void)
{
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdtr, idtr;
    __asm__ volatile("sgdt %0":"=m"(gdtr));
    __asm__ volatile("sidt %0":"=m"(idtr));
    LLHex("GDT base = ",gdtr.base); LLHex("GDT limit = ",gdtr.limit);
    LLHex("IDT base = ",idtr.base); LLHex("IDT limit = ",idtr.limit);
}

void LL_Serial(const PARSED_COMMAND *Command)
{
    uint64_t value=0;
    if(Command->ArgCount<1U || !LLNum(Command->Args[0],&value)) { LLLine("Usage: serial <text-as-hex>"); return; }
    uint16_t port=0x3F8;
    Outb(port,0x01);
    Outb(port+1,0x00);
    Outb(port+3,0x03);
    Outb(port+2,0xC7);
    Outb(port+4,0x0B);
    while(value) { Outb(port,(uint8_t)(value&0xFFU)); value>>=8; }
    LLLine("sent bytes to COM1");
}
