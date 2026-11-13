/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "acpi.h"
#include "include/nullboot.h"
#include "kernel/debug/debug.h"
#include "mm/vmm.h"

#include <stdint.h>
#include <stdbool.h>
#include "libc/string.h"

#define RSDP_MAGIC "RSD PTR "
#define ACPI_MAX_TABLE_SIZE (1024U * 1024U)
#define PAGE_SIZE_4K 0x1000ULL

typedef struct __attribute__((packed))
{
    char Signature[8];
    uint8_t Checksum;
    char OemId[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} RSDP2;

typedef struct __attribute__((packed))
{
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OemId[6];
    char OemTableId[8];
    uint32_t OemRevision;
    uint32_t CreatorId;
    uint32_t CreatorRevision;
} ACPI_SDT_HEADER;

static uint64_t XsdtAddr;
static uint32_t XsdtLength;
static bool Ready;

static bool AcpiChecksumValid(const void *Pointer, uint32_t Length)
{
    const uint8_t *Bytes = (const uint8_t *)Pointer;
    uint8_t Sum = 0;

    if (Pointer == 0 || Length == 0)
        return false;

    for (uint32_t Index = 0; Index < Length; Index++)
        Sum = (uint8_t)(Sum + Bytes[Index]);

    return Sum == 0;
}

/* ACPI table addresses are physical. Map every page before dereferencing it. */
static bool AcpiMapPhysicalRange(uint64_t Physical, uint64_t Length)
{
    if (Physical == 0 || Length == 0 || Physical + Length < Physical)
        return false;

    uint64_t FirstPage = Physical & ~(PAGE_SIZE_4K - 1ULL);
    uint64_t End = Physical + Length;
    if (End > UINT64_MAX - (PAGE_SIZE_4K - 1ULL))
        return false;

    End = (End + PAGE_SIZE_4K - 1ULL) & ~(PAGE_SIZE_4K - 1ULL);

    for (uint64_t Page = FirstPage; Page < End; Page += PAGE_SIZE_4K)
    {
        uint64_t Existing = VmmGetPhysical(Page);
        if (Existing == 0)
        {
            if (!VmmMapPage(Page, Page, VMM_P))
                return false;
        }
        else if (Existing != Page)
        {
            return false;
        }
    }

    return true;
}

void AcpiInitialize(void)
{
    Ready = false;
    XsdtAddr = 0;
    XsdtLength = 0;

    if (NullBoot == 0 || NullBoot->RSDP == 0)
        return;

    RSDP2 *Rsdp = (RSDP2 *)(uintptr_t)NullBoot->RSDP;
    if (memcmp(Rsdp->Signature, RSDP_MAGIC, sizeof(Rsdp->Signature)) != 0)
        return;

    if (!AcpiChecksumValid(Rsdp, 20U))
        return;

    if (Rsdp->Revision < 2U || Rsdp->Length < sizeof(RSDP2) ||
        Rsdp->Length > 4096U || !AcpiChecksumValid(Rsdp, Rsdp->Length) ||
        Rsdp->XsdtAddress == 0)
        return;

    if (!AcpiMapPhysicalRange(Rsdp->XsdtAddress, sizeof(ACPI_SDT_HEADER)))
        return;

    ACPI_SDT_HEADER *Xsdt =
        (ACPI_SDT_HEADER *)(uintptr_t)Rsdp->XsdtAddress;

    if (memcmp(Xsdt->Signature, "XSDT", 4) != 0 ||
        Xsdt->Length < sizeof(ACPI_SDT_HEADER) ||
        Xsdt->Length > ACPI_MAX_TABLE_SIZE ||
        ((Xsdt->Length - sizeof(ACPI_SDT_HEADER)) % sizeof(uint64_t)) != 0)
        return;

    if (!AcpiMapPhysicalRange(Rsdp->XsdtAddress, Xsdt->Length) ||
        !AcpiChecksumValid(Xsdt, Xsdt->Length))
        return;

    XsdtAddr = Rsdp->XsdtAddress;
    XsdtLength = Xsdt->Length;
    Ready = true;
    DbgInfo("ACPI XSDT online", (uintptr_t)XsdtAddr);
}

bool AcpiReady(void)
{
    return Ready;
}

uint64_t AcpiXsdt(void)
{
    return XsdtAddr;
}

uint64_t AcpiFindTable(const char Signature[4])
{
    if (!Ready || Signature == 0 || XsdtAddr == 0 ||
        XsdtLength < sizeof(ACPI_SDT_HEADER))
        return 0;

    ACPI_SDT_HEADER *Xsdt =
        (ACPI_SDT_HEADER *)(uintptr_t)XsdtAddr;
    uint32_t EntryCount =
        (XsdtLength - sizeof(ACPI_SDT_HEADER)) / sizeof(uint64_t);
    uint64_t *Entries =
        (uint64_t *)((uint8_t *)Xsdt + sizeof(ACPI_SDT_HEADER));

    for (uint32_t Index = 0; Index < EntryCount; Index++)
    {
        uint64_t Address = Entries[Index];
        if (Address == 0 ||
            !AcpiMapPhysicalRange(Address, sizeof(ACPI_SDT_HEADER)))
            continue;

        ACPI_SDT_HEADER *Table = (ACPI_SDT_HEADER *)(uintptr_t)Address;
        if (Table->Length < sizeof(ACPI_SDT_HEADER) ||
            Table->Length > ACPI_MAX_TABLE_SIZE ||
            !AcpiMapPhysicalRange(Address, Table->Length))
            continue;

        if (memcmp(Table->Signature, Signature, 4) == 0 &&
            AcpiChecksumValid(Table, Table->Length))
            return Address;
    }

    return 0;
}

uint64_t AcpiLocalApicAddress(void)
{
    typedef struct __attribute__((packed))
    {
        ACPI_SDT_HEADER Header;
        uint32_t LocalApicAddress;
        uint32_t Flags;
    } MADT_HEADER;

    MADT_HEADER *Madt =
        (MADT_HEADER *)(uintptr_t)AcpiFindTable("APIC");

    if (Madt == 0 || Madt->Header.Length < sizeof(MADT_HEADER))
        return 0;

    return Madt->LocalApicAddress;
}

/* Count enabled processors in MADT Local APIC and x2APIC records. */
uint64_t AcpiCpuCount(void)
{
    typedef struct __attribute__((packed))
    {
        ACPI_SDT_HEADER Header;
        uint32_t LocalApicAddress;
        uint32_t Flags;
    } MADT_HEADER;

    MADT_HEADER *Madt =
        (MADT_HEADER *)(uintptr_t)AcpiFindTable("APIC");
    if (Madt == 0 || Madt->Header.Length < sizeof(MADT_HEADER))
        return 0;

    const uint8_t *Entry =
        (const uint8_t *)Madt + sizeof(MADT_HEADER);
    const uint8_t *End =
        (const uint8_t *)Madt + Madt->Header.Length;
    uint64_t Count = 0;

    while ((uintptr_t)(End - Entry) >= 2U)
    {
        uint8_t Type = Entry[0];
        uint8_t Length = Entry[1];
        if (Length < 2U || (uintptr_t)(End - Entry) < Length)
            break;

        if (Type == 0U && Length >= 8U)
        {
            uint32_t Flags;
            memcpy(&Flags, Entry + 4U, sizeof(Flags));
            if (Flags & 1U)
                Count++;
        }
        else if (Type == 9U && Length >= 16U)
        {
            uint32_t Flags;
            memcpy(&Flags, Entry + 8U, sizeof(Flags));
            if (Flags & 1U)
                Count++;
        }

        Entry += Length;
    }

    return Count;
}
