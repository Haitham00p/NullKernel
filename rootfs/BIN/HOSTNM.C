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
#include <diskfs.h>

void main(void)
{
    char buf[256];
    uint32_t got = 0;

    if (!DiskFsChdirPath("/etc"))
    {
        puts("hostname: /etc missing");
        return;
    }
    if (!DiskFsRead("hostname", buf, (uint32_t)sizeof(buf), &got))
    {
        puts("hostname: /etc/hostname missing");
        DiskFsCdRoot();
        return;
    }
    buf[got] = '\0';
    printf("%s", buf);
    DiskFsCdRoot();
}
