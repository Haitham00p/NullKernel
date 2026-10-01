#ifndef NULLOS_NET_H
#define NULLOS_NET_H

#include <stdbool.h>
#include <stdint.h>

#define NET_IP(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))

void NetInitialize(void);
void NetPoll(void);
bool NetDhcpRequest(void);

/* Controller/stack bring-up split. NetInitialize() is registered with the
 * driver registry and only touches local state; NetBringUp() runs DHCP and
 * must be called from a context where a blocking exchange is acceptable. */
bool NetBringUp(void);
void NetService(void);

/* Link-layer status: TRUE when the controller is present and its rings are
 * programmed. This is independent of having an IPv4 address. */
bool NetLinkIsUp(void);
bool NetGetMac(uint8_t Mac[6]);

bool NetIsUp(void);
uint32_t NetGetIp(void);
uint32_t NetGetGateway(void);
uint32_t NetGetDns(void);
uint32_t NetGetNetmask(void);

bool NetPing(uint32_t Ip);
bool NetUdpSend(uint32_t Ip, uint16_t SrcPort, uint16_t DstPort,
                const void *Data, uint16_t Length);

/* DNS: returns an IPv4 address in network-stack byte order. */
bool NetDnsResolve(const char *Name, uint32_t *Address);

/* Minimal TCP host API. Poll NetPoll while waiting for state changes. */
bool NetTcpConnect(uint32_t Ip, uint16_t Port);
bool NetTcpSend(const void *Data, uint16_t Length);
bool NetTcpConnected(void);
void NetTcpClose(void);

#endif
