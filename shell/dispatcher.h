#ifndef NULL_DISPATCHER_H
#define NULL_DISPATCHER_H
#include "parser.h"
#include "../string/string.h"

extern uint32_t PromColor;

void DispatcherDispatch(const PARSED_COMMAND *Command);

void DispatcherInitialize(void);

#endif