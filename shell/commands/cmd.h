#ifndef CMD_H
#define CMD_H

#include "../parser.h"
#include "fs/ramfs/ramfs.h"
#include <stdint.h>

void CmdMsgFromNull(void); 
void CmdHelp(const char *Category);
void CmdAbout(void);
void CmdFill(const PARSED_COMMAND *Command);
void CmdNewLine(void);
void CmdEcho(const PARSED_COMMAND *command);
void CmdPurge(void);
void CmdDevKit(void);
void CmdDelReg(const char *Name);
void CmdfRegOut(const char *Name);
void CmdRegIn(const char *Name, const void *Data, uint64_t Size);
void CmdRegTo(const char *Name);
void CmdLs(void);
void CmdTouch(const char *Path);
void CmdCat(const char *Path);
void CmdRm(const char *Path);
void CmdWrite(const PARSED_COMMAND *Command);
void CmdCd(const char *Path);
void CmdMkdir(const char *Path);
void CmdRmdir(const char *Path);
void CmdPwd(void);
void CmdSuf(const char *CommandName);
void CmdEdit(const char *Path);
void CmdRbt(void);
void CmdShdn(void);
void CmdShowAddr(const char *API);
void CmdIsoList(void);

// Low-Level Memory & CPU Commands
void CmdPeek(const PARSED_COMMAND *Command);
void CmdPoke(const PARSED_COMMAND *Command);
void CmdHexDump(const PARSED_COMMAND *Command);
void CmdInB(const PARSED_COMMAND *Command);
void CmdOutB(const PARSED_COMMAND *Command);
void CmdInW(const PARSED_COMMAND *Command);
void CmdOutW(const PARSED_COMMAND *Command);
void CmdInD(const PARSED_COMMAND *Command);
void CmdOutD(const PARSED_COMMAND *Command);
void CmdCpuId(void);
void CmdRdtsc(void);
void CmdCR0(void);
void CmdCR3(void);
void CmdCR4(void);
void CmdMsr(const PARSED_COMMAND *Command);
void CmdDiskRead(const PARSED_COMMAND *Command);
void CmdTickTestPIT(void);
void CmdBep(uint64_t Freq, uint32_t Last);
void CmdSBepEmrg(void);
void CmdNSong(uint64_t Freq1, uint32_t Last1, uint64_t Freq2, uint32_t Last2, uint64_t Freq3, uint32_t Last3, uint64_t Freq4, uint32_t Last4, uint64_t Freq5, uint32_t Last5, uint64_t Freq6, uint32_t Last6);

// High-Level Utility & System Commands
void CmdNeofetch(void);
void CmdMatrix(void);
void CmdCalc(const PARSED_COMMAND *Command);
void CmdTree(void);
void CmdUptime(void);
void CmdTheme(const PARSED_COMMAND *Command);
void CmdIsoFind(const char *Name);
void CmdIsoCat(const char *Path);
void CmdIsoCopy(const char *IsoPath, const char *DiskPath);

#endif
