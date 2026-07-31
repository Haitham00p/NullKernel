#ifndef NULLOS_EDITOR_H
#define NULLOS_EDITOR_H

#include <stdbool.h>

bool EditorOpen(const char *Path);
bool EditorIsActive(void);
void EditorHandleKey(char Character, bool ControlHeld);

#endif
