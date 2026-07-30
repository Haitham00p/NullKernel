#ifndef NULLOS_SHELL_H
#define NULLOS_SHELL_H

#include <stdint.h>

#define SHELL_MAX_LENGTH 64
#define PROMPT "Developer@NullOS ~> "
#define PROMPT_LENGTH 19

void ShellInitialize(void);

void ShellUpdate(void);

extern uint32_t PromColor;
extern uint32_t BackSpaceColor;

#endif