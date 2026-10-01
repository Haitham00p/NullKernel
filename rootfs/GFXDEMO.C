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

#include <Graphic/graphic.h>

int main(void)
{
    int x = 320;
    int y = 200;
    int vx = 4;
    int vy = 3;
    int i;
    GfxClear(GfxColor(0, 0, 32));
    for (i = 0; i < 800; i++)
    {
        x = x + vx;
        y = y + vy;
        if (x < 10)    vx = -vx;
        if (x > 1200)  vx = -vx;
        if (y < 10)    vy = -vy;
        if (y > 760)   vy = -vy;
        GfxClear(GfxColor(0, 0, 32));
        GfxFillCircle(x, y, 20, GfxColor(255, 80, 80));
        GfxText(10, 10, "Graphic demo - Ctrl+C to exit", GfxColor(255, 255, 255));
        GfxPresent();
        GfxDelay(16);
        if (GfxAbort()) break;
    }
    return 0;
}
