#ifndef NULL_DISPATCHER_H
#define NULL_DISPATCHER_H
#include "parser.h"
#include "lib/string/string.h"

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;

void DispatcherDispatch(const PARSED_COMMAND *Command);

void DispatcherInitialize(void);

/* Runs /BIN/<Command>.EXE from the installed disk when present.
   Implemented in shell/selfhost.c. */
bool DispatcherTryDiskCommand(const PARSED_COMMAND *Command);

#endif
