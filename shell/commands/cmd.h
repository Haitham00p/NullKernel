#ifndef CMD_H
#define CMD_H

#include "../parser.h"
#include "fs/ramfs/ramfs.h"
#include <stdint.h>

void CmdMsgFromNull(void); 
void CmdHelp(void);
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

// High-Level Utility & System Commands
void CmdNeofetch(void);
void CmdMatrix(void);
void CmdCalc(const PARSED_COMMAND *Command);
void CmdTree(void);
void CmdUptime(void);
void CmdTheme(const PARSED_COMMAND *Command);
void CmdIsoFind(const char *Name);
void CmdIsoCat(const char *Path);
void CmdIsoCopy(const char *IsoPath, const char *VfsPath);

#endif
