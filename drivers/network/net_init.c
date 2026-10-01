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

#include "net.h"
#include "e1000/e1000.h"
#include "include/drivers.h"
#include "kernel/debug/debug.h"

/* Network bring-up runs in two stages.
 *
 * Stage 1 (here, at driver-registry time): bring the controller and the
 * protocol stack up. This is all non-blocking -- the registry runs before
 * the filesystem, the registry store and the shell, so nothing in this
 * function may wait on the wire.
 *
 * Stage 2 (from the kernel, once the shell is alive): run DHCP, which is a
 * blocking exchange with a server and therefore has no business delaying
 * the boot path.
 */

/* Stage 1: controller + stack. Registered after e1000_driver_init because
 * the Makefile link order puts the hardware driver first. */
static void net_stack_init(void)
{
    NetInitialize();
}

DRIVER_INIT(net_stack_init);

bool NetLinkIsUp(void)
{
    return E1000IsReady();
}

bool NetGetMac(uint8_t Mac[6])
{
    return E1000GetMac(Mac);
}

/*
 * Stage 2: obtain addressing over DHCP. Bounded by the stack's own retry
 * budget, so a missing or silent server returns false instead of hanging.
 */
bool NetBringUp(void)
{
    if (!E1000IsReady())
    {
        DbgInfo("NET: no controller", 0);
        return false;
    }

    if (NetIsUp())
        return true;

    if (!NetDhcpRequest())
    {
        DbgInfo("NET: DHCP failed, link stays unconfigured", 0);
        return false;
    }

    DbgInfo("NET: DHCP configured", NetGetIp());
    return true;
}

/*
 * Keep the receive path moving. The controller is polled, not interrupt
 * driven, so somebody has to drain the RX ring; without this ARP, DHCP,
 * TCP and DNS replies are never observed.
 */
void NetService(void)
{
    if (!E1000IsReady())
        return;

    NetPoll();
}