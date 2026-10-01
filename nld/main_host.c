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

/* nld — standalone host linker CLI
 *
 *   nld [-v vault.txt] -o out.elf in1.obj [in2.obj ...]
 *
 * vault.txt lines: "<hex address> <name>" (e.g. nm-style output).
 */
#include "nld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUTS 16
#define MAX_VAULT  4096

static uint8_t *ReadFile(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (f == NULL)
    {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0)
    {
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)n + 1U);
    if (buf != NULL && fread(buf, 1U, (size_t)n, f) != (size_t)n)
    {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *size = (uint32_t)n;
    return buf;
}

int main(int argc, char **argv)
{
    const char *out_path = NULL;
    const char *vault_path = NULL;
    const char *inputs[MAX_INPUTS];
    int input_count = 0;
    int i;

    static NldSymbol vault[MAX_VAULT];
    static char vault_names[MAX_VAULT][64];
    uint32_t vault_count = 0;

    static NldObject objs[MAX_INPUTS];
    static uint8_t *buffers[MAX_INPUTS];

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0 && i + 1 < argc)
        {
            vault_path = argv[++i];
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            out_path = argv[++i];
        }
        else if (input_count < MAX_INPUTS)
        {
            inputs[input_count++] = argv[i];
        }
    }

    if (out_path == NULL || input_count == 0)
    {
        fprintf(stderr,
                "usage: %s [-v vault.txt] -o out.elf a.obj [b.obj ...]\n",
                argv[0]);
        return 2;
    }

    if (vault_path != NULL)
    {
        FILE *f = fopen(vault_path, "r");
        if (f == NULL)
        {
            fprintf(stderr, "nld: cannot open vault '%s'\n", vault_path);
            return 2;
        }
        while (vault_count < MAX_VAULT &&
               fscanf(f, "%llx %63s",
                      (unsigned long long *)&vault[vault_count].Addr,
                      vault_names[vault_count]) == 2)
        {
            vault[vault_count].Name = vault_names[vault_count];
            vault_count++;
        }
        fclose(f);
    }

    for (i = 0; i < input_count; i++)
    {
        buffers[i] = ReadFile(inputs[i], &objs[i].Size);
        if (buffers[i] == NULL)
        {
            fprintf(stderr, "nld: cannot read '%s'\n", inputs[i]);
            return 2;
        }
        objs[i].Name = inputs[i];
        objs[i].Data = buffers[i];
    }

    {
        uint8_t *elf;
        uint32_t elf_size;
        NldDiag diag;

        if (!NldLink(objs, (uint32_t)input_count, vault, vault_count,
                     &elf, &elf_size, &diag))
        {
            fprintf(stderr, "nld: %s\n", diag.Message);
            return 1;
        }

        {
            FILE *f = fopen(out_path, "wb");
            if (f == NULL || fwrite(elf, 1U, elf_size, f) != elf_size)
            {
                fprintf(stderr, "nld: cannot write '%s'\n", out_path);
                return 2;
            }
            fclose(f);
        }
        printf("linked %u bytes -> %s\n", elf_size, out_path);
    }
    return 0;
}
