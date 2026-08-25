#ifndef NULLOS_EDITOR_H
#define NULLOS_EDITOR_H

#include <stdint.h>
#include <stdbool.h>

bool EditorOpen(const char *Path);

bool EditorIsActive(void);

void EditorHandleKey(uint8_t Key, bool ControlHeld);

#endif