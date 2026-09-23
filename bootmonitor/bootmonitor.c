#include "bootmonitor.h"
#include <stdint.h>
#include <stdbool.h>
#include "include/diskfs.h"
#include "kernel/terminal/terminal.h"
#include "drivers/input/keyboard/keyboard.h"
#include "mm/heap/heap.h"
#include "lib/string/string.h"
#include "arch/x86_64/power/pwr.h"
#include "bootmonitor/bootasm.h"
#include "n64a/n64a.h"

#define BM_DIR "/BOOT"
#define BM_REQ "BOOTMON.CFG"
#define BM_SEL "BOOTSEL.CFG"
#define BM_MAX 192U
#define BM_MAX_IMAGE (8U * 1024U * 1024U)
#define BM_ID_PREFIX "KRNL1.0:0x"
#define NPB_RED 0x00FF3030U
#define NPB_ORANGE 0x00FF8C00U
#define NPB_YELLOW 0x00FFFF00U
#define NPB_GREEN 0x0000FF00U
#define NPB_CYAN 0x0000FFFFU
#define NPB_DIM 0x00888888U

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
static void PC(const char *s,uint32_t color){TerminalPrintLine32(s,color);}

static bool KernelIdForName(const char *name, char *id, uint32_t cap, uint32_t *slot)
{
    char out[4096]; char *p;
    if(!name||!id||cap<16U||!DiskFsChdirPath(BM_DIR)) return false;
    if(!DiskFsListNames(out,sizeof(out))){DiskFsCdRoot();return false;}
    uint32_t current=0;
    p=out;
    while(*p){
        uint32_t l=0;while(p[l]&&p[l]!='\n'&&p[l]!='\r')l++;
        if(l){
            char n[96];uint32_t cp=l<95?l:95;for(uint32_t i=0;i<cp;i++)n[i]=p[i];n[cp]=0;
            if(ElfName(n)){
                uint32_t this_slot;
                if(strcasecmp(n,"kernel.elf")==0) this_slot=0;
                else { this_slot=current; if(current==0) current=1; }
                if(strcasecmp(n,"kernel.elf")!=0 && this_slot==0) this_slot=1;
                if(strcasecmp(n,name)==0){
                    if(slot)*slot=this_slot;
                    const char digs[]="0123456789ABCDEF";char hex[24];uint32_t hv=this_slot,hn=0,pos=0;
                    const char *pre=BM_ID_PREFIX;while(pre[pos]&&pos+1<cap)id[pos]=pre[pos],pos++;
                    do{hex[hn++]=digs[hv&15U];hv>>=4;}while(hv);
                    while(hn){if(pos+1>=cap){DiskFsCdRoot();return false;}id[pos++]=hex[--hn];}
                    id[pos]=0;DiskFsCdRoot();return true;
                }
                if(strcasecmp(n,"kernel.elf")!=0 && current<0xFFFFFFFFU) current++;
            }
        }
        p+=l;while(*p=='\n'||*p=='\r')p++;
    }
    DiskFsCdRoot();return false;
}

static bool NameForKernelId(const char *id, char *name, uint32_t cap)
{
    if(!id||!name||cap<2U||strncasecmp(id,BM_ID_PREFIX,10)!=0)return false;
    const char *h=id+10;if(!*h)return false;
    uint32_t wanted=0;
    while(*h){char ch=*h++;uint32_t v;
        if(ch>='0'&&ch<='9')v=(uint32_t)(ch-'0');
        else if(ch>='a'&&ch<='f')v=(uint32_t)(ch-'a'+10);
        else if(ch>='A'&&ch<='F')v=(uint32_t)(ch-'A'+10);
        else return false;
        if(wanted>0x0FFFFFFFU)return false;
        wanted=(wanted<<4)|v;
    }
    char out[4096];char *p;
    if(!DiskFsChdirPath(BM_DIR)||!DiskFsListNames(out,sizeof(out))){DiskFsCdRoot();return false;}
    uint32_t slot=0;p=out;
    while(*p){
        uint32_t l=0;while(p[l]&&p[l]!='\n'&&p[l]!='\r')l++;
        if(l){
            char n[96];uint32_t cp=l<95?l:95;for(uint32_t i=0;i<cp;i++)n[i]=p[i];n[cp]=0;
            if(ElfName(n)){
                uint32_t this_slot=(strcasecmp(n,"kernel.elf")==0)?0U:slot;
                if(strcasecmp(n,"kernel.elf")!=0&&slot==0)slot=1,this_slot=1;
                if(this_slot==wanted){
                    uint32_t i=0;while(i+1<cap&&n[i]){name[i]=n[i];i++;}name[i]=0;
                    DiskFsCdRoot();return true;
                }
                if(strcasecmp(n,"kernel.elf")!=0&&slot<0xFFFFFFFFU)slot++;
            }
        }
        p+=l;while(*p=='\n'||*p=='\r')p++;
    }
    DiskFsCdRoot();return false;
}

static bool ReadBootConfig(char *out,uint32_t cap)
{
    uint8_t b[4096];uint32_t got=0,i;
    if(cap<2U || !DiskFsReadInPath("/BOOT/LIMINE","limine.conf",b,sizeof(b)-1,&got)) return false;
    b[got]=0;
    const char *needle="path:";
    for(i=0;i+5<got;i++) if(b[i]=='p'&&b[i+1]=='a'&&b[i+2]=='t'&&b[i+3]=='h'&&b[i+4]==':'){
        uint32_t s=i+5;while(s<got&&(b[s]==' '||b[s]=='\\t'))s++;
        uint32_t e=s;while(e<got&&b[e]!='\\n'&&b[e]!='\\r')e++;
        uint32_t n=e-s;if(n>=cap)n=cap-1;for(uint32_t j=0;j<n;j++)out[j]=(char)b[s+j];out[n]=0;return true;
    }
    return false;
}

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
    PC("READY KERNELS / NPB IMAGE REGISTRY",NPB_ORANGE);
    n=DiskFsListNames(out,sizeof(out));p=out;
    while(*p){
        uint32_t l=0;while(p[l]&&p[l]!='\n'&&p[l]!='\r')l++;
        if(l){
            char name[96];uint32_t i,cp=l<95?l:95;uint32_t sz=0;
            for(i=0;i<cp;i++)name[i]=p[i];name[cp]=0;
            if(ElfName(name)&&DiskFsFileSize(name,&sz)){
                char kid[32]; uint32_t kslot=0;
                KernelIdForName(name,kid,sizeof(kid),&kslot);
                DiskFsChdirPath(BM_DIR);
                TerminalWrite32("  ",0x00FFFFFFU);
                TerminalWrite32(kid,NPB_CYAN);
                TerminalWrite32("  ",0x00FFFFFFU);
                TerminalWrite32(name,0x00FFFFFFU);
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

static bool VerifyLoaded(void)
{
    uint64_t e,l,h;
    if(!g_image){PC("verify: nothing staged",NPB_YELLOW);return false;}
    if(!Validate(g_image,g_size,&e,&l,&h)){PC("verify: STAGED IMAGE FAILED VALIDATION",NPB_RED);return false;}
    PC("verify: staged ELF is valid",NPB_GREEN);return true;
}

static void PrintStatus(void)
{
    PC("NPB STATUS",NPB_ORANGE);
    TerminalWrite32("  staged ID     : ",NPB_DIM);
    if(g_name[0]){char id[32];uint32_t slot=0;if(KernelIdForName(g_name,id,sizeof(id),&slot))TerminalPrintLine32(id,NPB_CYAN);else TerminalPrintLine32("unknown",NPB_YELLOW);}
    else TerminalPrintLine32("none",NPB_YELLOW);
    TerminalWrite32("  staged file   : ",NPB_DIM);TerminalPrintLine32(g_name[0]?g_name:"none",0x00FFFFFFU);
    TerminalWrite32("  staged bytes  : ",NPB_DIM);TerminalPrintHex64Compact(g_image?g_size:0);TerminalNewLine32(0);
    TerminalWrite32("  entry         : ",NPB_DIM);TerminalPrintHex64Compact(g_image?g_entry:0);TerminalNewLine32(0);
    char path[256];
    TerminalWrite32("  Limine path   : ",NPB_DIM);
    if(ReadBootConfig(path,sizeof(path)))TerminalPrintLine32(path,NPB_CYAN);else TerminalPrintLine32("unreadable",NPB_RED);
}

static void PrintDanger(void)
{
    PC("!! DANGER ZONE !!",NPB_RED);
    PC("NPB can change the next kernel and execute raw x86-64.",NPB_YELLOW);
    PC("There is no shell safety layer here.",NPB_ORANGE);
    PC("RSP/RIP remain monitor-owned in jmp-a.",NPB_DIM);
}

static void ContinueBoot(void){
    const char *name=g_name[0]?g_name:"kernel.elf";
    if(!SafeName(name)){P("continue: invalid kernel name");return;}
    if(!VerifyLoaded() && g_name[0]) return;
    if(!DiskFsWriteInPath(BM_DIR,BM_SEL,name,Len(name))){P("jmp: cannot save selection");return;}
    if(!StageLimine(name)){
        P("jmp: selection saved, but installed Limine config was not changed");
        P("jmp: boot source may be ISO/CD; NPB cannot rewrite a read-only ISO.");
    }else{
        TerminalWrite32("JMP -> ",NPB_ORANGE);TerminalPrintLine32(name,NPB_GREEN);
    }
    Reboot();
}

static void Help(void){
    PC("NullOS PreBoot (NPB) command set",NPB_ORANGE);
    P("  help                    show this help");
    P("  ShowKernels             list /BOOT/*.ELF");
    P("  load <KRNL1.0:0xN>       validate and stage a kernel by ID");
    P("  unload                  discard staged kernel");
    P("  jmp <ID>                stage/select kernel ID and reboot");
    P("  jmp                     reboot into staged/default kernel");
    P("  jmp-a                   enter raw instruction AsmMode");
    P("  meminfo                 show staged image size");
    P("  status                  show NPB state");
    P("  verify                  revalidate staged ELF");
    P("  config                  show current Limine kernel path");
    P("  kernels                 alias for ShowKernels");
    P("  danger                  show dangerous-mode warning");
    P("  ids                     explain kernel IDs");
    P("  map                     show staged ELF range");
    P("  entry                   show staged ELF entry");
    P("  reboot                  reboot now");
    P("  halt                    halt CPU");
    P("  about                   show NPB identity");
    P("  clear-selection         forget staged kernel");
    P("  clear-request           clear one-shot NPB request");
    P("  poweroff                halt after disabling interrupts");
    P("  regs/reset              inspect/reset AsmMode registers");
}

static void ReadLine(char *line,uint32_t cap);

static BootAsmRegs AsmRegs;

static void AsmPrintReg(const char *name, uint64_t v)
{
    TerminalWrite32(name, 0x00808080U);
    TerminalWrite32("=0x", 0x00808080U);
    TerminalPrintHex64Compact(v);
    TerminalNewLine32(0);
}

static void AsmRegsPrint(void)
{
    AsmPrintReg("rax",AsmRegs.rax); AsmPrintReg("rbx",AsmRegs.rbx);
    AsmPrintReg("rcx",AsmRegs.rcx); AsmPrintReg("rdx",AsmRegs.rdx);
    AsmPrintReg("rsi",AsmRegs.rsi); AsmPrintReg("rdi",AsmRegs.rdi);
    AsmPrintReg("rbp",AsmRegs.rbp); AsmPrintReg("r8 ",AsmRegs.r8);
    AsmPrintReg("r9 ",AsmRegs.r9);  AsmPrintReg("r10",AsmRegs.r10);
    AsmPrintReg("r11",AsmRegs.r11); AsmPrintReg("r12",AsmRegs.r12);
    AsmPrintReg("r13",AsmRegs.r13); AsmPrintReg("r14",AsmRegs.r14);
    AsmPrintReg("r15",AsmRegs.r15);
}

static void AsmHelp(void)
{
    P("AsmMode - one native x86-64 instruction per line");
    P("  help                    show AsmMode help");
    P("  regs                    show register state");
    P("  reset                   zero all exposed registers");
    P("  back                    return to BootMonitor");
    P("  <instruction>           assemble and execute it immediately");
    P("  RSP and RIP are owned by the monitor");
    P("  Example: mov rax, 5");
    P("  Example: add rax, 7");
    P("  Example: outb 0x64, 0xFE");
}

static void AsmExecuteInstruction(const char *line)
{
    char src[BM_MAX + 16];
    uint8_t *obj = NULL;
    uint32_t obj_size = 0;
    N64aDiag diag;
    N64ObjHeader hdr;
    uint32_t text_size;
    uint8_t *code;
    uint32_t n = Len(line);

    if (!n || n + 5U >= sizeof(src)) { P("AsmMode: instruction too long"); return; }

    for (uint32_t i=0;i<n;i++) src[i]=line[i];
    src[n++]='\n'; src[n++]='r'; src[n++]='e'; src[n++]='t'; src[n]='\0';

    if (!N64aAssemble("<boot-asm>", src, n, &obj, &obj_size, &diag))
    {
        TerminalWrite32("asm: ",0x00FF5555U);
        TerminalPrintLine32(diag.Message,0x00FFFFFFU);
        return;
    }

    if (obj_size < N64O_HDRSZ || obj[0]!='N' || obj[1]!='6' ||
        obj[2]!='4' || obj[3]!='O' || obj[4]!='B' || obj[5]!='J')
    {
        kfree(obj); P("AsmMode: assembler returned invalid object"); return;
    }

    for (uint32_t i=0;i<sizeof(hdr);i++) ((uint8_t*)&hdr)[i]=obj[i];
    text_size=hdr.TextSize;
    if (!text_size || text_size > 256U ||
        (uint64_t)N64O_HDRSZ + text_size > obj_size)
    {
        kfree(obj); P("AsmMode: invalid text section"); return;
    }

    code=(uint8_t*)kmalloc(text_size);
    if(!code){kfree(obj);P("AsmMode: out of memory");return;}
    for(uint32_t i=0;i<text_size;i++) code[i]=obj[N64O_HDRSZ+i];

    BootAsmRun(code,&AsmRegs);
    kfree(code);
    kfree(obj);
    AsmRegsPrint();
}

static void AsmMode(void)
{
    char line[BM_MAX];

    P("");
    P("AsmMode: native instruction executor online.");
    P("Type help for commands. back returns to BootMonitor.");
    P("RSP/RIP are monitor-owned.");
    AsmRegsPrint();

    for(;;)
    {
        TerminalWrite32("AsmMode> ",0x0000FFFFU);
        ReadLine(line,sizeof(line));

        char *p=line;
        while(*p==' '||*p=='\t')p++;
        if(Eq(p,"back")||Eq(p,"exit")||Eq(p,"quit")) return;
        if(Eq(p,"help")){AsmHelp();continue;}
        if(Eq(p,"regs")){AsmRegsPrint();continue;}
        if(Eq(p,"reset")){for(uint32_t i=0;i<sizeof(AsmRegs);i++)((uint8_t*)&AsmRegs)[i]=0;AsmRegsPrint();continue;}
        if(!*p)continue;
        AsmExecuteInstruction(p);
    }
}
static void Execute(char *line){
    char *cmd=line,*arg=NULL;
    while(*cmd==' '||*cmd=='\t')cmd++; if(!*cmd)return;
    for(char *p=cmd;*p;p++)if(*p==' '||*p=='\t'){*p=0;arg=p+1;while(*arg==' '||*arg=='\t')arg++;break;}
    if(Eq(cmd,"help")){Help();return;}
    if(Eq(cmd,"ShowKernels")||Eq(cmd,"showkernels")){ShowKernels();return;}
    if(Eq(cmd,"load")){if(!arg||!*arg)P("Usage: load <KRNL1.0:0xID>");else{
        char resolved[96];
        if(!NameForKernelId(arg,resolved,sizeof(resolved))){P("load: unknown kernel ID");return;}
        arg=resolved;Unload();
        if(!DiskFsChdirPath(BM_DIR)){P("load: /BOOT unavailable");return;}
        if(!ReadKernel(arg)){DiskFsCdRoot();P("load: rejected - invalid/unreadable NullOS kernel ELF");return;}
        DiskFsCdRoot();
    }return;}
    if(Eq(cmd,"unload")){
        if(!arg||!*arg){P("Usage: unload <KRNL1.0:0xN>");return;}
        char resolved[96],current_id[32];uint32_t slot=0;
        if(!g_name[0]||!KernelIdForName(g_name,current_id,sizeof(current_id),&slot)){P("unload: nothing staged");return;}
        if(!NameForKernelId(arg,resolved,sizeof(resolved))||strcasecmp(resolved,g_name)!=0){P("unload: ID does not match the staged kernel");return;}
        Unload();P("unloaded");return;}
    if(Eq(cmd,"jmp")){
        if(arg&&*arg){
            char resolved[96];
            if(!NameForKernelId(arg,resolved,sizeof(resolved))){P("jmp: unknown kernel ID");return;}
            Unload();
            if(!DiskFsChdirPath(BM_DIR)||!ReadKernel(resolved)){DiskFsCdRoot();P("jmp: failed to stage selected kernel");return;}
            DiskFsCdRoot();
        }
        ContinueBoot();return;
    }
    if(Eq(cmd,"jmp-a")){AsmMode();return;}
    if(Eq(cmd,"status")){PrintStatus();return;}
    if(Eq(cmd,"verify")){VerifyLoaded();return;}
    if(Eq(cmd,"config")){char path[256];if(ReadBootConfig(path,sizeof(path)))TerminalPrintLine32(path,NPB_CYAN);else P("config: unreadable");return;}
    if(Eq(cmd,"kernels")){ShowKernels();return;}
    if(Eq(cmd,"danger")){PrintDanger();return;}
    if(Eq(cmd,"ids")){P("IDs use KRNL1.0:0xN; kernel.elf is KRNL1.0:0x0.");return;}
    if(Eq(cmd,"about")){PC("NullOS PreBoot (NPB) // RAW BOOT CONTROL",NPB_ORANGE);PC("Disk-side preboot environment. No normal shell safety layer.",NPB_RED);return;}
    if(Eq(cmd,"clear-selection")){Unload();P("staged kernel cleared");return;}
    if(Eq(cmd,"clear-request")){BootMonitorClearRequest();P("NPB request cleared");return;}
    if(Eq(cmd,"poweroff")){__asm__ volatile("cli");for(;;)__asm__ volatile("hlt");}

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
void BootMonitorClearRequest(void)
{
    if(!DiskFsIsMounted()) return;
    bool removed=false;
    if(DiskFsChdirPath(BM_DIR))
    {
        removed=DiskFsRemove(BM_REQ);
        DiskFsCdRoot();
    }
    if(!removed)
    {
        const char off[]="BM0\\n";
        /* Fallback: an explicit disabled marker is safer than a stale BM1
           request that can trap the machine in NPB forever. */
        DiskFsWriteInPath(BM_DIR,BM_REQ,off,4);
    }
}
void BootMonitorRun(void){
    char line[BM_MAX];
    BootMonitorClearRequest();
    PC("",NPB_DIM);
    PC("==============================================================",NPB_RED);
    PC("                    NullOS PreBoot",NPB_ORANGE);
    PC("                         [ NPB ]",NPB_RED);
    PC("==============================================================",NPB_RED);
    PC("!! DANGER ZONE // RAW BOOT CONTROL // NO SAFETY NET !!",NPB_YELLOW);
    PC("This environment can select the next kernel or execute native code.",NPB_DIM);
    PC("",NPB_DIM);
    ShowKernels();
    PC("",NPB_DIM);
    for(;;){TerminalWrite32("NPB> ",NPB_CYAN);ReadLine(line,sizeof(line));Execute(line);}
}
