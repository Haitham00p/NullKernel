#include "bootmonitor.h"
#include <stdint.h>
#include <stdbool.h>
#include "include/diskfs.h"
#include "kernel/terminal/terminal.h"
#include "drivers/input/keyboard/keyboard.h"
#include "mm/heap/heap.h"
#include "lib/string/string.h"
#include "arch/x86_64/power/pwr.h"

#define BM_DIR "/BOOT"
#define BM_REQ "BOOTMON.CFG"
#define BM_SEL "BOOTSEL.CFG"
#define BM_MAX 192U
#define BM_MAX_IMAGE (8U * 1024U * 1024U)

typedef struct {
    uint8_t id[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} BM_EHDR;

typedef struct {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
} BM_PHDR;

static uint8_t *g_image;
static uint32_t g_size;
static uint64_t g_entry;
static char g_name[96];

static uint32_t Len(const char *s) { uint32_t n=0; while(s && s[n]) n++; return n; }
static bool Eq(const char *a,const char *b){return strcasecmp(a,b)==0;}
static bool ElfName(const char *s){
    uint32_t n=Len(s); return n>=4 && s[n-4]=='.' &&
      (s[n-3]=='e'||s[n-3]=='E') && (s[n-2]=='l'||s[n-2]=='L') &&
      (s[n-1]=='f'||s[n-1]=='F');
}
static bool SafeName(const char *s){
    uint32_t i,n=Len(s); if(!n||n>=sizeof(g_name)||!ElfName(s))return false;
    for(i=0;i<n;i++){char c=s[i]; if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
      (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;} return true;
}
static void P(const char *s){TerminalPrintLine32(s,0x00FFFFFFU);}

static bool Validate(const uint8_t *img,uint32_t size,uint64_t *entry,
                     uint64_t *lo,uint64_t *hi){
    const BM_EHDR *e; uint32_t i; bool have=false; uint64_t l=UINT64_MAX,h=0;
    if(!img||size<sizeof(BM_EHDR))return false; e=(const BM_EHDR*)img;
    if(e->id[0]!=0x7f||e->id[1]!='E'||e->id[2]!='L'||e->id[3]!='F'||
       e->id[4]!=2||e->id[5]!=1||e->id[6]!=1||e->type!=2||e->machine!=62||
       e->version!=1||e->ehsize!=64||e->phentsize!=56||!e->phnum)return false;
    if(e->phoff>size||(uint64_t)e->phnum*56ULL>(uint64_t)size-e->phoff)return false;
    if(e->entry<0xffffffff80000000ULL||e->entry>=0xffffffffc0000000ULL)return false;
    for(i=0;i<e->phnum;i++){
        const BM_PHDR *p=(const BM_PHDR*)(img+e->phoff+(uint64_t)i*56ULL);
        if(p->type!=1)continue; have=true;
        if(p->filesz>p->memsz||p->offset>size||p->filesz>(uint64_t)size-p->offset)return false;
        if(p->vaddr<0xffffffff80000000ULL||p->vaddr+p->memsz<p->vaddr||
           p->vaddr+p->memsz>0xffffffffc0000000ULL)return false;
        if(!p->align||(p->align&(p->align-1ULL)))return false;
        if(p->align>1&&((p->vaddr-p->offset)&(p->align-1ULL)))return false;
        if(p->vaddr<l)l=p->vaddr; if(p->vaddr+p->memsz>h)h=p->vaddr+p->memsz;
    }
    if(!have||l==UINT64_MAX||h<=l)return false;
    *entry=e->entry;*lo=l;*hi=h;return true;
}

static void Unload(void){
    if(g_image)kfree(g_image); g_image=NULL;g_size=0;g_entry=0;g_name[0]=0;
}

static bool ReadKernel(const char *name){
    uint32_t n=0; uint8_t *buf; uint64_t lo,hi;
    if(!DiskFsFileSize(name,&n)||!n||n>BM_MAX_IMAGE)return false;
    buf=(uint8_t*)kmalloc(n); if(!buf)return false;
    if(!DiskFsRead(name,buf,n,&n)){kfree(buf);return false;}
    if(!Validate(buf,n,&g_entry,&lo,&hi)){kfree(buf);return false;}
    g_image=buf;g_size=n;strcpy(g_name,name);
    TerminalWrite32("loaded ",0x0000FF00U);TerminalWrite32(g_name,0x00FFFFFFU);
    TerminalWrite32(" entry=",0x00808080U);TerminalPrintHex64Compact(g_entry);
    TerminalWrite32(" range=",0x00808080U);TerminalPrintHex64Compact(lo);
    TerminalWrite32("-",0x00808080U);TerminalPrintHex64Compact(hi);TerminalNewLine32(0);
    return true;
}

static void ShowKernels(void){
    char out[4096];uint32_t n;char *p;
    if(!DiskFsChdirPath(BM_DIR)){P("ShowKernels: /BOOT unavailable");return;}
    P("READY KERNELS");
    n=DiskFsListNames(out,sizeof(out));p=out;
    while(*p){
        uint32_t l=0;while(p[l]&&p[l]!='\n'&&p[l]!='\r')l++;
        if(l){
            char name[96];uint32_t i,cp=l<95?l:95;uint32_t sz=0;
            for(i=0;i<cp;i++)name[i]=p[i];name[cp]=0;
            if(ElfName(name)&&DiskFsFileSize(name,&sz)){
                TerminalWrite32("  ",0x00FFFFFFU);TerminalWrite32(name,0x0000FFFFU);
                TerminalWrite32("  ",0x00FFFFFFU);TerminalPrintHex64Compact(sz);
                TerminalPrintLine32(" bytes",0x00808080U);
            }
        }
        p+=l;while(*p=='\n'||*p=='\r')p++;
    }
    if(!n)P("  (none)");DiskFsCdRoot();
}

static bool StageLimine(const char *name){
    uint8_t *buf=(uint8_t*)kmalloc(4096);uint32_t got=0,i,j,n=Len(name);
    const char *needle="path: boot():/boot/";const uint32_t nl=19;
    if(!buf||n>80){if(buf)kfree(buf);return false;}
    if(!DiskFsReadInPath("/BOOT/LIMINE","limine.conf",buf,4095,&got)){kfree(buf);return false;}
    for(i=0;i+nl<got;i++){
        bool ok=true;for(j=0;j<nl;j++)if(buf[i+j]!=(uint8_t)needle[j]){ok=false;break;}
        if(!ok)continue;
        uint32_t end=i+nl;while(end<got&&buf[end]!='\n'&&buf[end]!='\r')end++;
        uint32_t old=end-(i+nl),tail=got-end,newgot=got-old+n;
        if(newgot>=4095){kfree(buf);return false;}
        if(n>old)for(j=tail;j;j--)buf[end+(n-old)+j-1]=buf[end+j-1];
        else if(n<old)for(j=0;j<tail;j++)buf[end-(old-n)+j]=buf[end+j];
        got=newgot;for(j=0;j<n;j++)buf[i+nl+j]=(uint8_t)name[j];
        bool wr=DiskFsWriteInPath("/BOOT/LIMINE","limine.conf",buf,got);kfree(buf);return wr;
    }
    kfree(buf);return false;
}

static void ContinueBoot(void){
    const char *name=g_name[0]?g_name:"kernel.elf";
    if(!SafeName(name)){P("continue: invalid kernel name");return;}
    if(!DiskFsWriteInPath(BM_DIR,BM_SEL,name,Len(name))){P("continue: cannot save selection");return;}
    if(!StageLimine(name)){
        P("continue: selection saved, but installed Limine config was not changed");
        P("continue: ISO/CD boot remains unchanged because ISO is read-only");
    }else{
        TerminalWrite32("continuing with ",0x00FFFFFFU);TerminalPrintLine32(name,0x0000FF00U);
    }
    Reboot();
}

static void Help(void){
    P("Null Boot Monitor");
    P("  ShowKernels             list /BOOT/*.ELF");
    P("  load <kernel.elf>       validate and stage a kernel");
    P("  unload                  discard staged kernel");
    P("  continue                reboot into staged/default kernel");
    P("  continue-a              enter AsmMode");
    P("  meminfo                 show staged image size");
    P("  map                     show staged ELF range");
    P("  entry                   show staged ELF entry");
    P("  reboot                  reboot");
    P("  halt                    halt CPU");
}
static void Execute(char *line){
    char *cmd=line,*arg=NULL;
    while(*cmd==' '||*cmd=='\t')cmd++; if(!*cmd)return;
    for(char *p=cmd;*p;p++)if(*p==' '||*p=='\t'){*p=0;arg=p+1;while(*arg==' '||*arg=='\t')arg++;break;}
    if(Eq(cmd,"help")){Help();return;}
    if(Eq(cmd,"ShowKernels")||Eq(cmd,"showkernels")){ShowKernels();return;}
    if(Eq(cmd,"load")){if(!arg||!*arg)P("Usage: load <kernel.elf>");else{
        if(!SafeName(arg)){P("load: invalid kernel filename");return;}Unload();
        if(!DiskFsChdirPath(BM_DIR)){P("load: /BOOT unavailable");return;}
        if(!ReadKernel(arg)){DiskFsCdRoot();P("load: rejected - invalid/unreadable NullOS kernel ELF");return;}
        DiskFsCdRoot();
    }return;}
    if(Eq(cmd,"unload")){Unload();P("unloaded");return;}
    if(Eq(cmd,"continue")){ContinueBoot();return;}
    if(Eq(cmd,"continue-a")){P("AsmMode>");P("native instruction execution is not enabled yet; assembly-mode kernel support is staged separately");return;}
    if(Eq(cmd,"meminfo")){TerminalWrite32("loaded=",0x00FFFFFFU);TerminalPrintHex64Compact(g_image?g_size:0);TerminalPrintLine32(" bytes",0x00808080U);return;}
    if(Eq(cmd,"map")){uint64_t e,l,h;if(!g_image){P("map: nothing loaded");return;}if(Validate(g_image,g_size,&e,&l,&h)){TerminalPrintHex64Compact(l);TerminalWrite32(" - ",0x00808080U);TerminalPrintHex64Compact(h);TerminalNewLine32(0);}return;}
    if(Eq(cmd,"entry")){if(!g_image)P("entry: nothing loaded");else{TerminalWrite32("entry=",0x00FFFFFFU);TerminalPrintHex64Compact(g_entry);TerminalNewLine32(0);}return;}
    if(Eq(cmd,"reboot")){Reboot();return;}
    if(Eq(cmd,"halt")){__asm__ volatile("cli");for(;;)__asm__ volatile("hlt");}
    P("unknown command; type help");
}

static void ReadLine(char *line,uint32_t cap){
    uint32_t n=0;for(;;){uint8_t k=KbdReadKey();
        if(k=='\n'||k=='\r'){line[n]=0;TerminalNewLine32(0);return;}
        if(k==8||k==127){if(n){n--;line[n]=0;TerminalWrite32("\b \b",0x00FFFFFFU);}continue;}
        if(k>=0x20&&k<=0x7e&&n+1<cap){line[n++]=(char)k;line[n]=0;char s[2]={(char)k,0};TerminalWrite32(s,0x00FFFFFFU);}
    }
}

bool BootMonitorShouldEnter(void){
    uint8_t m[8];uint32_t got=0;
    return DiskFsIsMounted()&&DiskFsReadInPath(BM_DIR,BM_REQ,m,sizeof(m),&got)&&
           got>=4&&m[0]=='B'&&m[1]=='M'&&m[2]=='1'&&m[3]=='\n';
}
void BootMonitorRequest(void){const char m[]="BM1\n";DiskFsWriteInPath(BM_DIR,BM_REQ,m,4);}
void BootMonitorClearRequest(void){if(DiskFsIsMounted())DiskFsRemove(BM_REQ);}
void BootMonitorRun(void){
    char line[BM_MAX];BootMonitorClearRequest();
    P("");P("==============================================");P("          NULL BOOT MONITOR 1.0");
    P("==============================================");P("Disk-side boot control. type help.");P("");
    for(;;){TerminalWrite32("BootMonitor> ",0x0000FFFFU);ReadLine(line,sizeof(line));Execute(line);}
}
