#ifndef INSTALLER_H
#define INSTALLER_H

typedef enum
{
    /* User chose "Try OS" (live mode) - continue into the normal shell. */
    INSTALLER_RESULT_LIVE,
    /* Installation was requested but failed.  Do NOT continue into the
       normal shell; the disk may be half-written. */
    INSTALLER_RESULT_FAILED,
    /* Installation completed; the disk is now bootable.  Reboot. */
    INSTALLER_RESULT_SUCCEEDED
} InstallerResult;

/*
 * Live-CD installer entry point.
 * Shows the "(Try OS or Install?)" menu and waits for a key.
 */
InstallerResult InstallerRun(void);

#endif