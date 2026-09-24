#include "registry.h"
#include "include/diskfs.h"
#include "lib/string/string.h"
#include <stdint.h>
#define REG_VERSION 2U
static bool g_Ready;
static bool SplitKey(const char *KeyPath,char *Dir,uint32_t DirSize,char *Name,uint32_t NameSize)
{
    const char *slash=0,*p; uint32_t dlen;
    if(!KeyPath||KeyPath[0]!='/'||!Dir||!Name) return false;
    for(p=KeyPath;*p;p++) if(*p=='/') slash=p;
    if(!slash||slash==KeyPath||!slash[1]) return false;
    dlen=(uint32_t)(slash-KeyPath);
    if(dlen>=DirSize||strlen(slash+1)>=NameSize) return false;
    memcpy(Dir,KeyPath,dlen); Dir[dlen]='\0'; strcpy(Name,slash+1); return true;
}
bool RegistryEnsureLayout(void)
{
    static const char *dirs[]={
        "/REG","/REG/color","/REG/SYSTEM","/REG/SHELL","/REG/BOOT",
        "/REG/PROCESS","/REG/SCHED","/REG/SYSCALL","/REG/MEMORY",
        "/REG/ACPI","/REG/PCI","/REG/NullOS","/REG/NullOS/RAB",
        "/REG/NullOS/ROB","/REG/NullOS/TASKS"
    };
    if(!DiskFsIsMounted()) return false;
    for(uint32_t i=0;i<sizeof(dirs)/sizeof(dirs[0]);i++)
        if(!DiskFsMkdirAll(dirs[i])) return false;
    return true;
}
bool RegistryPathExists(const char *KeyPath)
{
    char d[DISKFS_PATH_MAX],n[DISKFS_PATH_MAX]; FAT32DirectoryEntry e;
    return SplitKey(KeyPath,d,sizeof(d),n,sizeof(n))&&DiskFsFindInPath(d,n,&e);
}
bool RegistryReadText(const char *KeyPath,char *Out,uint32_t MaxOut)
{
    char d[DISKFS_PATH_MAX],n[DISKFS_PATH_MAX]; uint32_t got=0;
    if(!Out||MaxOut==0||!SplitKey(KeyPath,d,sizeof(d),n,sizeof(n))) return false;
    if(!DiskFsReadInPath(d,n,Out,MaxOut-1U,&got)) return false;
    Out[got]='\0'; return true;
}
bool RegistryWriteText(const char *KeyPath,const char *Value)
{
    char d[DISKFS_PATH_MAX],n[DISKFS_PATH_MAX];
    if(!Value||!SplitKey(KeyPath,d,sizeof(d),n,sizeof(n))) return false;
    return DiskFsWriteInPath(d,n,Value,(uint32_t)strlen(Value));
}
static int HexDigit(char c)
{
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
bool RegistryReadU64(const char *KeyPath,uint64_t *Value)
{
    char b[40]; uint64_t v=0; uint32_t i=0; int n;
    if(!Value||!RegistryReadText(KeyPath,b,sizeof(b))) return false;
    if(b[0]=='0'&&(b[1]=='x'||b[1]=='X')) i=2;
    if(i==2&&b[i]=='\0') return false;
    for(;b[i];i++){
        if(b[i]=='\n'||b[i]=='\r'||b[i]==' '||b[i]=='\t') break;
        n=HexDigit(b[i]); if(n<0) return false; v=(v<<4)|(uint64_t)n;
    }
    *Value=v; return true;
}
bool RegistryWriteU64(const char *KeyPath,uint64_t Value)
{
    static const char hex[]="0123456789ABCDEF"; char b[19]; uint32_t p=18;
    b[p]='\0'; do{b[--p]=hex[Value&0xF];Value>>=4;}while(Value);
    b[--p]='x';b[--p]='0'; return RegistryWriteText(KeyPath,&b[p]);
}
bool RegistryInitialize(void)
{
    if(!DiskFsIsMounted()||!RegistryEnsureLayout()) return false;
    if(!RegistryPathExists("/REG/SYSTEM/VERSION.V")) RegistryWriteU64("/REG/SYSTEM/VERSION.V",REG_VERSION);
    if(!RegistryPathExists("/REG/SCHED/QUANTUM.V")) RegistryWriteU64("/REG/SCHED/QUANTUM.V",10);
    if(!RegistryPathExists("/REG/PROCESS/MAX.V")) RegistryWriteU64("/REG/PROCESS/MAX.V",32);
    if(!RegistryPathExists("/REG/SYSCALL/VERSION.V")) RegistryWriteU64("/REG/SYSCALL/VERSION.V",1);
    if(!RegistryPathExists("/REG/MEMORY/PAGE_SIZE.V")) RegistryWriteU64("/REG/MEMORY/PAGE_SIZE.V",4096);
    if(!RegistryPathExists("/REG/BOOT/ROB.ENABLED.V")) RegistryWriteText("/REG/BOOT/ROB.ENABLED.V","1");
    if(!RegistryPathExists("/REG/BOOT/RAB.ENABLED.V")) RegistryWriteText("/REG/BOOT/RAB.ENABLED.V","1");
    if(!RegistryPathExists("/REG/NullOS/ROB/README.V"))
        RegistryWriteText("/REG/NullOS/ROB/README.V","ELF executables in this directory run at boot.");
    g_Ready=true; return true;
}
uint32_t RegistryVersion(void){return g_Ready?REG_VERSION:0;}
