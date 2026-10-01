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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ncc/ncc.h"

static int Run(const char *path, int object_mode)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", path);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = 0;
    fclose(f);
    unsigned char *img = NULL;
    unsigned int img_size = 0;
    int ok;
    if (object_mode)
    {
        ok = NccCompileObject(path, buf, (unsigned int)got,
                              &img, &img_size);
    }
    else
    {
        ok = NccCompile(path, buf, (unsigned int)got, &img, &img_size);
    }
    fprintf(stderr, "RESULT: ok=%d image_size=%u\n", ok, img_size);
    if (ok && img_size > 0)
    {
        char out[512];
        snprintf(out, sizeof(out), "%s.out", path);
        FILE *o = fopen(out, "wb");
        fwrite(img, 1, img_size, o);
        fclose(o);
        fprintf(stderr, "wrote %s (%u bytes)\n", out, img_size);
    }
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    int object_mode = 0;
    const char *path = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--object") == 0)
        {
            object_mode = 1;
        }
        else
        {
            path = argv[i];
        }
    }
    if (path == NULL)
    {
        fprintf(stderr, "usage: ncc_host [--object] <file.c>\n");
        return 2;
    }
    return Run(path, object_mode);
}
