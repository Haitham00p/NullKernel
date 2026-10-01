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

/* n64a — standalone host assembler CLI */
#include "n64a.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *ReadFile(const char *path, uint32_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;
    if (f == NULL)
    {
        fprintf(stderr, "n64a: cannot open '%s'\n", path);
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
    else if (buf != NULL)
    {
        buf[n] = '\0'; /* NUL-terminate for convenience */
    }
    fclose(f);
    *size = (uint32_t)n;
    return buf;
}

int main(int argc, char **argv)
{
    const char *out_path = NULL;
    const char *in_path = NULL;
    int i;
    int list = 0;
    uint8_t *src;
    uint32_t src_size;
    uint8_t *obj;
    uint32_t obj_size;
    N64aDiag diag;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            out_path = argv[++i];
        }
        else if (strcmp(argv[i], "-l") == 0)
        {
            list = 1; /* dump object sections to stdout */
        }
        else
        {
            in_path = argv[i];
        }
    }
    if (in_path == NULL || out_path == NULL)
    {
        fprintf(stderr,
                "usage: %s [-l] -o <output.obj> <input.asm>\n", argv[0]);
        return 2;
    }

    src = ReadFile(in_path, &src_size);
    if (src == NULL)
    {
        return 2;
    }

    if (!N64aAssemble(in_path, (const char *)src, src_size,
                      &obj, &obj_size, &diag))
    {
        fprintf(stderr, "n64a: %s\n", diag.Message);
        free(src);
        return 1;
    }
    if (diag.Failed)
    {
        fprintf(stderr, "n64a: line %u: %s\n", diag.Line, diag.Message);
        free(src);
        return 1;
    }

    if (list != 0)
    {
        uint32_t textsz = obj[16] | ((uint32_t)obj[17] << 8) |
                          ((uint32_t)obj[18] << 16) |
                          ((uint32_t)obj[19] << 24);
        uint32_t datasz = obj[20] | ((uint32_t)obj[21] << 8) |
                          ((uint32_t)obj[22] << 16) |
                          ((uint32_t)obj[23] << 24);
        printf("object: %u bytes, text=%u data=%u has_main=%u\n",
               obj_size, textsz, datasz, obj[12]);
        for (i = 0; i < (int)textsz; i++)
        {
            printf("%02x ", obj[N64O_HDRSZ + i]);
            if ((i % 16) == 15 || i == (int)textsz - 1)
            {
                printf("\n");
            }
        }
    }

    {
        FILE *f = fopen(out_path, "wb");
        if (f == NULL ||
            fwrite(obj, 1U, obj_size, f) != obj_size)
        {
            fprintf(stderr, "n64a: cannot write '%s'\n", out_path);
            free(src);
            free(obj);
            return 2;
        }
        fclose(f);
    }

    free(src);
    free(obj);
    return 0;
}
