#ifndef INSTALLER_H
#define INSTALLER_H

#include <stdbool.h>

/*
 * Live-CD installer entry point.
 * Shows the "(Try OS or Install?)" menu and waits for a key.
 * Returns true if the user requested an install.
 */
bool InstallerRun(void);

#endif
