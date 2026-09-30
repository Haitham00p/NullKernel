#include "net.h"
#include "e1000/e1000.h"
#include "kernel/debug/debug.h"

#include <stdint.h>
#include <stdbool.h>

#define ETH_MTU 1518
#define ETH_HDR 14
#define ETH_ARP 0x0806
#define ETH_IPV4 0x0800
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17
#define ARP_HTYPE_ETH 1
#define ARP_PTYPE_IPV4 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8
#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DNS_PORT 53
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_MSS 1460

static uint8_t g_Mac[6];
static uint32_t g_Ip;
static uint32_t g_Mask;
static uint32_t g_Gateway;
static uint32_t g_Dns;
static uint16_t g_IpId;
static uint32_t g_Rng = 0x4E554C4C;

static void HandleUdp(const IpHdr *ip,const uint8_t *p,uint16_t len);

static uint8_t g_ArpMac[6];
static uint32_t g_ArpIp;
static bool g_ArpValid;

static uint16_t g_UdpSrcPort = 49152;

static struct {
    bool active;
    bool connected;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint16_t local_port;
    uint32_t snd_nxt;
    uint32_t snd_una;
    uint32_t rcv_nxt;
} g_Tcp;

typedef struct __attribute__((packed)) {
    uint8_t dst[6]; uint8_t src[6]; uint16_t type;
} EthHdr;

typedef struct __attribute__((packed)) {
    uint16_t htype; uint16_t ptype; uint8_t hlen; uint8_t plen;
    uint16_t oper; uint8_t sha[6]; uint32_t spa; uint8_t tha[6]; uint32_t tpa;
} ArpPkt;

typedef struct __attribute__((packed)) {
    uint8_t vhl; uint8_t tos; uint16_t len; uint16_t id; uint16_t frag;
    uint8_t ttl; uint8_t proto; uint16_t sum; uint32_t src; uint32_t dst;
} IpHdr;

typedef struct __attribute__((packed)) {
    uint8_t type; uint8_t code; uint16_t sum; uint16_t id; uint16_t seq;
} IcmpHdr;

typedef struct __attribute__((packed)) {
    uint16_t src; uint16_t dst; uint16_t len; uint16_t sum;
} UdpHdr;

typedef struct __attribute__((packed)) {
    uint16_t src; uint16_t dst; uint32_t seq; uint32_t ack;
    uint8_t off; uint8_t flags; uint16_t window; uint16_t sum; uint16_t urg;
} TcpHdr;

typedef struct __attribute__((packed)) {
    uint8_t op; uint8_t htype; uint8_t hlen; uint8_t hops;
    uint32_t xid; uint16_t secs; uint16_t flags;
    uint32_t ciaddr; uint32_t yiaddr; uint32_t siaddr; uint32_t giaddr;
    uint8_t chaddr[16]; uint8_t sname[64]; uint8_t file[128];
} DhcpFixed;

static uint16_t Be16(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static uint32_t Be32(uint32_t x) { return ((x & 0xFFU) << 24) | ((x & 0xFF00U) << 8) | ((x >> 8) & 0xFF00U) | ((x >> 24) & 0xFFU); }
static uint16_t Rd16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t Rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static void Cp(uint8_t *d, const uint8_t *s, uint16_t n) { for (uint16_t i=0;i<n;i++) d[i]=s[i]; }
static void Z(uint8_t *d, uint16_t n) { for (uint16_t i=0;i<n;i++) d[i]=0; }

static uint16_t Sum16(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    while (len > 1) { sum += ((uint16_t)p[0] << 8) | p[1]; p += 2; len -= 2; }
    if (len) sum += (uint16_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFU) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint16_t TransportSum(uint32_t src, uint32_t dst, uint8_t proto,
                             const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    const uint8_t *a = (const uint8_t *)&src;
    const uint8_t *b = (const uint8_t *)&dst;
    sum += ((uint16_t)a[0] << 8) | a[1]; sum += ((uint16_t)a[2] << 8) | a[3];
    sum += ((uint16_t)b[0] << 8) | b[1]; sum += ((uint16_t)b[2] << 8) | b[3];
    sum += proto; sum += len;
    for (uint16_t i=0;i+1<len;i+=2) sum += ((uint16_t)data[i]<<8)|data[i+1];
    if (len & 1) sum += (uint16_t)data[len-1] << 8;
    while (sum >> 16) sum=(sum&0xFFFFU)+(sum>>16);
    return (uint16_t)~sum;
}

static bool EtherSend(const uint8_t dst[6], uint16_t type,
                      const void *payload, uint16_t len)
{
    uint8_t frame[ETH_MTU];
    EthHdr *h = (EthHdr *)frame;
    if (len > ETH_MTU - ETH_HDR) return false;
    Cp(h->dst,dst,6); Cp(h->src,g_Mac,6); h->type=Be16(type);
    Cp(frame+ETH_HDR,(const uint8_t *)payload,len);
    return E1000SendFrame(frame,(uint16_t)(ETH_HDR+len));
}

static bool ArpRequest(uint32_t ip)
{
    uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    ArpPkt a; a.htype=Be16(ARP_HTYPE_ETH); a.ptype=Be16(ARP_PTYPE_IPV4);
    a.hlen=6; a.plen=4; a.oper=Be16(ARP_REQUEST); Cp(a.sha,g_Mac,6); a.spa=g_Ip;
    Z(a.tha,6); a.tpa=ip;
    return EtherSend(bcast,ETH_ARP,&a,sizeof(a));
}

static bool ArpSendReply(const ArpPkt *q)
{
    ArpPkt a; a.htype=Be16(ARP_HTYPE_ETH); a.ptype=Be16(ARP_PTYPE_IPV4);
    a.hlen=6; a.plen=4; a.oper=Be16(ARP_REPLY); Cp(a.sha,g_Mac,6); a.spa=g_Ip;
    Cp(a.tha,q->sha,6); a.tpa=q->spa;
    return EtherSend(q->sha,ETH_ARP,&a,sizeof(a));
}

static bool ArpResolve(uint32_t ip)
{
    if (g_ArpValid && g_ArpIp == ip) return true;
    g_ArpValid=false;
    ArpRequest(ip);
    for (uint32_t i=0;i<100000;i++) { NetPoll(); if(g_ArpValid && g_ArpIp==ip) return true; __asm__ volatile("pause"); }
    return false;
}

static uint32_t Route(uint32_t dst)
{
    if ((dst & g_Mask) == (g_Ip & g_Mask)) return dst;
    return g_Gateway;
}

static bool IpSend(uint32_t dst, uint8_t proto, const void *data, uint16_t len)
{
    uint8_t p[ETH_MTU-ETH_HDR]; IpHdr *h=(IpHdr*)p;
    uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    const uint8_t *next_mac;
    if (len > sizeof(p)-sizeof(IpHdr)) return false;

    /*
     * DHCP starts before we have an IPv4 address, so a DHCP packet must be
     * sent directly to the Ethernet broadcast address. Never ARP for
     * 255.255.255.255 (or for 0.0.0.0 while configuring the interface).
     */
    if (dst == NET_IP(255,255,255,255))
    {
        next_mac = bcast;
    }
    else
    {
        uint32_t next=Route(dst);
        if (!ArpResolve(next)) return false;
        next_mac = g_ArpMac;
    }

    h->vhl=0x45; h->tos=0; h->len=Be16((uint16_t)(sizeof(IpHdr)+len)); h->id=Be16(g_IpId++);
    h->frag=Be16(0x4000); h->ttl=64; h->proto=proto; h->sum=0; h->src=g_Ip; h->dst=dst;
    Cp(p+sizeof(IpHdr),(const uint8_t*)data,len); h->sum=Sum16(h,sizeof(IpHdr));
    return EtherSend(next_mac,ETH_IPV4,p,(uint16_t)(sizeof(IpHdr)+len));
}

static bool IcmpSendEcho(uint32_t dst)
{
    uint8_t p[64]; IcmpHdr *h=(IcmpHdr*)p; h->type=ICMP_ECHO_REQUEST; h->code=0; h->sum=0; h->id=Be16(0x4E55); h->seq=Be16(1);
    for (uint16_t i=sizeof(IcmpHdr);i<sizeof(p);i++) p[i]=(uint8_t)i;
    h->sum=Sum16(p,sizeof(p));
    return IpSend(dst,IP_PROTO_ICMP,p,sizeof(p));
}

static bool UdpBuild(uint32_t dst,uint16_t src,uint16_t port,const void*data,uint16_t len)
{
    uint8_t p[1500]; UdpHdr *u=(UdpHdr*)p;
    if(len>sizeof(p)-sizeof(UdpHdr)) return false;
    u->src=Be16(src); u->dst=Be16(port); u->len=Be16((uint16_t)(len+sizeof(UdpHdr))); u->sum=0;
    Cp(p+sizeof(UdpHdr),(const uint8_t*)data,len);
    u->sum=TransportSum(g_Ip,dst,IP_PROTO_UDP,p,(uint16_t)(len+sizeof(UdpHdr)));
    if(u->sum==0) u->sum=0xFFFF;
    return IpSend(dst,IP_PROTO_UDP,p,(uint16_t)(len+sizeof(UdpHdr)));
}

bool NetUdpSend(uint32_t ip,uint16_t src,uint16_t dst,const void*data,uint16_t len)
{ return UdpBuild(ip,src,dst,data,len); }

static void HandleArp(const uint8_t *p,uint16_t len)
{
    if(len < sizeof(ArpPkt)) return;
    const ArpPkt *a=(const ArpPkt*)p;
    if(Be16(a->ptype)!=ARP_PTYPE_IPV4 || a->hlen!=6 || a->plen!=4) return;
    g_ArpIp=a->spa; Cp(g_ArpMac,a->sha,6); g_ArpValid=true;
    if(Be16(a->oper)==ARP_REQUEST && a->tpa==g_Ip) ArpSendReply(a);
}

static void HandleIcmp(const IpHdr *ip,const uint8_t*p,uint16_t len)
{
    if(len < sizeof(IcmpHdr)) return;
    const IcmpHdr *h=(const IcmpHdr*)p;
    if(h->type==ICMP_ECHO_REQUEST && ip->dst==g_Ip)
    {
        uint8_t out[1500]; if(len>sizeof(out)) return; Cp(out,p,len); IcmpHdr *r=(IcmpHdr*)out;
        r->type=ICMP_ECHO_REPLY; r->sum=0; r->sum=Sum16(out,len); IpSend(ip->src,IP_PROTO_ICMP,out,len);
    }
}

static void HandleTcp(const IpHdr *ip,const uint8_t*p,uint16_t len)
{
    if(len < sizeof(TcpHdr)) return;
    const TcpHdr *t=(const TcpHdr*)p;
    uint16_t off=(uint16_t)((t->off>>4)*4); if(off<20 || off>len) return;
    uint8_t flags=t->flags;
    if(!g_Tcp.active || ip->src!=g_Tcp.remote_ip || Be16(t->src)!=g_Tcp.remote_port) return;
    if((flags&TCP_SYN) && (flags&TCP_ACK)) { g_Tcp.rcv_nxt=Be32(t->seq)+1; g_Tcp.snd_una=Be32(t->ack); g_Tcp.connected=true; }
    else if(flags&TCP_FIN) { g_Tcp.rcv_nxt=Be32(t->seq)+1; g_Tcp.connected=false; }
}

static void HandleIpv4(const uint8_t*p,uint16_t len)
{
    if(len < sizeof(IpHdr)) return;
    const IpHdr *h=(const IpHdr*)p;
    if((h->vhl>>4)!=4 ||
       (h->dst!=g_Ip && h->dst!=NET_IP(255,255,255,255))) return;
    uint16_t ihl=(uint16_t)((h->vhl&15)*4); if(ihl<20 || ihl>len) return;
    uint16_t total=Be16(h->len); if(total>len || total<ihl) return;
    const uint8_t *body=p+ihl; uint16_t blen=(uint16_t)(total-ihl);
    if(h->proto==IP_PROTO_ICMP) HandleIcmp(h,body,blen);
    else if(h->proto==IP_PROTO_TCP) HandleTcp(h,body,blen);
    else if(h->proto==IP_PROTO_UDP) HandleUdp(h,body,blen);
}

void NetPoll(void)
{
    uint8_t frame[ETH_MTU]; uint16_t len=0;
    while(E1000ReceiveFrame(frame,sizeof(frame),&len))
    {
        if(len < sizeof(EthHdr)) continue;
        const EthHdr *e=(const EthHdr*)frame;
        uint16_t type=Be16(e->type); const uint8_t *p=frame+sizeof(EthHdr); uint16_t plen=(uint16_t)(len-sizeof(EthHdr));
        if(type==ETH_ARP) HandleArp(p,plen); else if(type==ETH_IPV4) HandleIpv4(p,plen);
    }
}

bool NetPing(uint32_t ip)
{ return IcmpSendEcho(ip); }

typedef struct
{
    bool waiting;
    bool offered;
    uint32_t xid;
    uint32_t offered_ip;
    uint32_t server_ip;
} DhcpState;

static DhcpState g_Dhcp;

static void DhcpBuild(uint8_t *p,uint32_t xid,uint8_t type,
                      uint32_t requested_ip,uint32_t server_ip)
{
    DhcpFixed *d=(DhcpFixed*)p;
    Z(p,300);
    d->op=1;
    d->htype=1;
    d->hlen=6;
    d->xid=xid;
    d->flags=Be16(0x8000);
    Cp(d->chaddr,g_Mac,6);

    uint8_t *o=p+sizeof(DhcpFixed);
    o[0]=99;o[1]=130;o[2]=83;o[3]=99;o+=4;

    o[0]=53;o[1]=1;o[2]=type;o+=3;       /* DHCP message type. */
    o[0]=55;o[1]=3;o[2]=1;o[3]=3;o[4]=6;o+=5; /* Mask/router/DNS. */

    if(type==3 && requested_ip!=0)
    {
        o[0]=50;o[1]=4;
        o[2]=(uint8_t)requested_ip;
        o[3]=(uint8_t)(requested_ip>>8);
        o[4]=(uint8_t)(requested_ip>>16);
        o[5]=(uint8_t)(requested_ip>>24);
        o+=6;

        if(server_ip!=0)
        {
            o[0]=54;o[1]=4;
            o[2]=(uint8_t)server_ip;
            o[3]=(uint8_t)(server_ip>>8);
            o[4]=(uint8_t)(server_ip>>16);
            o[5]=(uint8_t)(server_ip>>24);
            o+=6;
        }
    }

    o[0]=255;
}

static bool DhcpParse(const uint8_t *p,uint16_t len,uint32_t xid,
                      uint8_t *message_type,uint32_t *server_ip)
{
    if(len<sizeof(DhcpFixed)+4) return false;
    const DhcpFixed *d=(const DhcpFixed*)p;
    if(d->xid!=xid || d->op!=2 || d->yiaddr==0) return false;

    const uint8_t *o=p+sizeof(DhcpFixed);
    if(o[0]!=99||o[1]!=130||o[2]!=83||o[3]!=99) return false;
    o+=4;

    uint8_t mt=0;
    uint32_t mask=0, gateway=0, dns=0, sid=0;
    uint32_t pos=(uint32_t)(o-p);
    uint32_t end=(uint32_t)len;

    while(pos+1<end)
    {
        uint8_t code=o[0];
        if(code==255) break;
        if(code==0){o++;pos++;continue;}

        uint8_t n=o[1];
        if(pos+2U+n>end) return false;

        if(code==53 && n==1) mt=o[2];
        else if(code==1 && n==4) mask=Rd32(o+2);
        else if(code==3 && n>=4) gateway=Rd32(o+2);
        else if(code==6 && n>=4) dns=Rd32(o+2);
        else if(code==54 && n>=4) sid=Rd32(o+2);

        o+=2+n;
        pos+=2+n;
    }

    if(message_type) *message_type=mt;
    if(server_ip) *server_ip=sid;

    if(mt==2) /* OFFER */
    {
        g_Dhcp.offered_ip=d->yiaddr;
        g_Dhcp.server_ip=sid;
        g_Dhcp.offered=true;
        return true;
    }

    if(mt==5) /* ACK */
    {
        g_Ip=d->yiaddr;
        g_Mask=mask;
        g_Gateway=gateway;
        g_Dns=dns;
        g_Dhcp.server_ip=sid;
        return true;
    }

    return false;
}

static void HandleUdp(const IpHdr *ip,const uint8_t *p,uint16_t len)
{
    if(len < sizeof(UdpHdr)) return;
    (void)ip;
    const UdpHdr *u=(const UdpHdr*)p;
    uint16_t src=Be16(u->src);
    uint16_t dst=Be16(u->dst);
    uint16_t total=Be16(u->len);

    if(total<sizeof(UdpHdr) || total>len) return;

    if(g_Dhcp.waiting && src==DHCP_SERVER_PORT && dst==DHCP_CLIENT_PORT)
    {
        uint8_t type=0;
        uint32_t server=0;
        (void)DhcpParse(p+sizeof(UdpHdr),(uint16_t)(total-sizeof(UdpHdr)),
                        g_Dhcp.xid,&type,&server);
    }
}

bool NetDhcpRequest(void)
{
    if(!E1000IsReady()) return false;

    g_Ip=0;
    g_Mask=0;
    g_Gateway=0;
    g_Dns=0;
    g_ArpValid=false;

    g_Dhcp.waiting=true;
    g_Dhcp.offered=false;
    g_Dhcp.xid=++g_Rng;
    g_Dhcp.offered_ip=0;
    g_Dhcp.server_ip=0;

    uint8_t p[300];
    DhcpBuild(p,g_Dhcp.xid,1,0,0); /* DISCOVER */

    if(!UdpBuild(NET_IP(255,255,255,255),DHCP_CLIENT_PORT,
                 DHCP_SERVER_PORT,p,300))
    {
        g_Dhcp.waiting=false;
        return false;
    }

    for(uint32_t i=0;i<300000;i++)
    {
        NetPoll();

        if(g_Dhcp.offered)
            break;

        __asm__ volatile("pause");
    }

    if(!g_Dhcp.offered)
    {
        g_Dhcp.waiting=false;
        return false;
    }

    DhcpBuild(p,g_Dhcp.xid,3,g_Dhcp.offered_ip,g_Dhcp.server_ip); /* REQUEST */

    if(!UdpBuild(NET_IP(255,255,255,255),DHCP_CLIENT_PORT,
                 DHCP_SERVER_PORT,p,300))
    {
        g_Dhcp.waiting=false;
        return false;
    }

    for(uint32_t i=0;i<300000;i++)
    {
        NetPoll();

        if(g_Ip!=0)
        {
            g_Dhcp.waiting=false;
            return true;
        }

        __asm__ volatile("pause");
    }

    g_Dhcp.waiting=false;
    return false;
}

static bool TcpPacket(uint32_t dst,uint16_t sport,uint16_t dport,uint32_t seq,uint32_t ack,uint8_t flags,const void*data,uint16_t len)
{
    uint8_t p[1500]; TcpHdr*t=(TcpHdr*)p; if(len>sizeof(p)-20)return false; t->src=Be16(sport);t->dst=Be16(dport);t->seq=Be32(seq);t->ack=Be32(ack);t->off=0x50;t->flags=flags;t->window=Be16(8192);t->sum=0;t->urg=0;Cp(p+20,(const uint8_t*)data,len);t->sum=TransportSum(g_Ip,dst,IP_PROTO_TCP,p,(uint16_t)(20+len));return IpSend(dst,IP_PROTO_TCP,p,(uint16_t)(20+len));
}

bool NetTcpConnect(uint32_t ip,uint16_t port)
{
    if(g_Tcp.active) return false;
    g_Tcp.active=true;
    g_Tcp.connected=false;
    g_Tcp.remote_ip=ip;
    g_Tcp.remote_port=port;
    g_Tcp.local_port=++g_UdpSrcPort;
    g_Tcp.snd_nxt=++g_Rng;
    if(!TcpPacket(ip,g_Tcp.local_port,port,g_Tcp.snd_nxt,0,TCP_SYN,0,0)){g_Tcp.active=false;return false;}g_Tcp.snd_nxt++;
    for(uint32_t i=0;i<500000&&!g_Tcp.connected;i++){NetPoll();__asm__ volatile("pause");}
    return g_Tcp.connected;
}

bool NetTcpSend(const void*data,uint16_t len)
{
    if(!g_Tcp.connected || len>TCP_MSS) return false;
    if(!TcpPacket(g_Tcp.remote_ip,g_Tcp.local_port,g_Tcp.remote_port,
                  g_Tcp.snd_nxt,g_Tcp.rcv_nxt,TCP_ACK|TCP_PSH,data,len))
        return false;
    g_Tcp.snd_nxt+=len;
    return true;
}

bool NetTcpConnected(void){return g_Tcp.connected;}
void NetTcpClose(void){if(g_Tcp.active)TcpPacket(g_Tcp.remote_ip,g_Tcp.local_port,g_Tcp.remote_port,g_Tcp.snd_nxt,g_Tcp.rcv_nxt,TCP_FIN|TCP_ACK,0,0);g_Tcp.active=false;g_Tcp.connected=false;}

bool NetDnsResolve(const char *name,uint32_t *address)
{
    /* DNS packet construction is intentionally kept bounded: one QNAME, A/IN query. */
    if(!name || !address || !g_Dns) return false;
    uint8_t q[512];
    Z(q,sizeof(q));
    uint16_t id=++g_IpId;
    q[0]=(uint8_t)(id>>8); q[1]=(uint8_t)id; q[2]=1; q[5]=1;
    uint16_t pos=12;
    const char*s=name;while(*s){const char*dot=s;while(*dot&&*dot!='.')dot++;uint8_t n=(uint8_t)(dot-s);if(n>63||(uint32_t)pos+n+1U >= (uint32_t)sizeof(q)-5U)return false;q[pos++]=n;for(uint8_t i=0;i<n;i++)q[pos++]=(uint8_t)s[i];s=*dot?dot+1:dot;}q[pos++]=0;q[pos++]=0;q[pos++]=1;q[pos++]=0;q[pos++]=1;
    uint16_t src=++g_UdpSrcPort;if(!UdpBuild(g_Dns,src,DNS_PORT,q,pos))return false;
    /* DNS reply reception is handled by the normal polling path; this compact API returns false until a matching reply is observed. */
    for(uint32_t t=0;t<200000;t++){uint8_t f[ETH_MTU];uint16_t l=0;NetPoll();if(E1000ReceiveFrame(f,sizeof(f),&l)&&l>42){EthHdr*e=(EthHdr*)f;if(Be16(e->type)==ETH_IPV4){IpHdr*ip=(IpHdr*)(f+14);if(ip->proto==IP_PROTO_UDP){UdpHdr*u=(UdpHdr*)((uint8_t*)ip+20);uint16_t ul=Be16(u->len);if(Be16(u->src)==53&&ul>=8+12){uint8_t*r=(uint8_t*)u+8;if(((uint16_t)r[0]<<8|r[1])==id&&r[3]&0x80){uint16_t an=(uint16_t)r[6]<<8|r[7];uint16_t x=12;while(x<ul-8&&r[x])x+=(uint8_t)r[x]+1;x+=5;for(uint16_t a=0;a<an&&x+12<ul-8;a++){while(x<ul-8&&r[x])x+=(uint8_t)r[x]+1;x+=1;if(x+10>ul-8)break;uint16_t typ=Rd16(r+x);uint16_t rd=Rd16(r+x+8);x+=10;if(typ==1&&rd==4&&x+4<=ul-8){*address=Rd32(r+x);return true;}x+=rd;}}}}}}__asm__ volatile("pause");}return false;
}

void NetInitialize(void)
{
    /*
     * E1000 is a hardware driver and is initialized by its own DRIVER_INIT
     * entry. Do not initialize it again here: doing so allocates a second
     * set of RX/TX rings and resets the controller during kernel startup.
     *
     * Network configuration is deliberately not performed from the driver
     * registry. DHCP is a blocking protocol operation and must not be able
     * to stall the kernel before the shell starts.
     */
    g_Ip=0;
    g_Mask=0;
    g_Gateway=0;
    g_Dns=0;
    g_ArpValid=false;
    g_Tcp.active=false;

    if(!E1000IsReady())
    {
        DbgInfo("NET: E1000 unavailable",0);
        return;
    }

    if(!E1000GetMac(g_Mac))
    {
        DbgInfo("NET: E1000 MAC unavailable",0);
        return;
    }

    g_Dhcp.waiting=false;
    g_Dhcp.offered=false;

    DbgInfo("NET: stack ready",(uintptr_t)g_Mac[0]);
}

bool NetIsUp(void){return E1000IsReady()&&g_Ip!=0;}
uint32_t NetGetIp(void){return g_Ip;}
uint32_t NetGetGateway(void){return g_Gateway;}
uint32_t NetGetDns(void){return g_Dns;}
uint32_t NetGetNetmask(void){return g_Mask;}
