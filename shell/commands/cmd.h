#ifndef CMD_H
#define CMD_H
#include "../parser.h"
#include "fs/ramfs/ramfs.h"
void CmdMsgFromNull(); 
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
#endif
