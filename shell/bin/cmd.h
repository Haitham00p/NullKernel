#ifndef CMD_H
#define CMD_H
#include "../parser.h"
#include "../ramfs/ramfs.h"
void CmdMsgFromNull(); 
void CmdHelp(void);
void CmdAbout(void);
void CmdFill(PARSED_COMMAND *Command);
void CmdNewLine(void);
void CmdEcho(PARSED_COMMAND *command);
void CmdPurge(void);
void CmdDevKit(void);
void CmdDelReg(const char *Name);
void CmdfRegOut(const char *Name);
void CmdRegIn(const char *Name, const void *Data, uint64_t Size);
void CmdRegTo(const char *Name);
#endif