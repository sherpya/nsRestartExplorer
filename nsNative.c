/*
 * NSIS Plugin to gracefully restart explorer
 *
 * Copyright (c) 2008 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this software; if not, write to the
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "nsRestartExplorer.h"

BOOL StartExplorer(DWORD timeout)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    PVOID OldValue = NULL;
    BOOL redirOk = FALSE;
    OutputDebugStringA("nsRE::StartExplorer");

    /* Disable Wow64 Redirection so we'll able to start 64bit explorer */
    if (pW64NoRedir) redirOk = pW64NoRedir(&OldValue);

    memset(&pi, 0, sizeof(PROCESS_INFORMATION));
    memset(&si, 0, sizeof(STARTUPINFO));

    si.cb = sizeof(STARTUPINFO);

    if(!CreateProcessA(NULL,    /* No module name (use command line) */
        SHELL,                  /* Command line */
        NULL,                   /* Process handle not inheritable */
        NULL,                   /* Thread handle not inheritable */
        FALSE,                  /* Set handle inheritance to FALSE */
        0,                      /* No creation flags */
        NULL,                   /* Use parent's environment block */
        NULL,                   /* Use parent's starting directory */
        &si,                    /* Pointer to STARTUPINFO structure */
        &pi))                   /* Pointer to PROCESS_INFORMATION structure */
    {
        if (pW64Revert) pW64Revert(OldValue);
        NS_FAILED(NULL, "Cannot spawn explorer process");
    }

    /* Revert FS Redirection since may interfere with the Setup */
    if (pW64Revert && redirOk) pW64Revert(OldValue);

    switch (WaitForInputIdle(pi.hProcess, timeout))
    {
        case 0            : break; /* OK */
        case WAIT_TIMEOUT :
            if (timeout == IGNORE) break; /* OK as requested */
            NS_FAILED(pi.hProcess, "Timeout while waiting for explorer process");
        case WAIT_FAILED  : NS_FAILED(pi.hProcess, "Error while waiting for explorer process");
        default           : NS_FAILED(pi.hProcess, "This should not be reached");
    }

    return TRUE;
}

BOOL QuitExplorer(DWORD timeout)
{
    HWND explWin = NULL;
    HANDLE explProc = NULL;
    DWORD pid = -1;

    OutputDebugStringA("nsRE::QuitExplorer");

    if (!(explWin = FindWindowA("Progman", NULL)))
        NS_FAILED(explProc, "Cannot find explorer window");

    GetWindowThreadProcessId(explWin, &pid);

    if (!(explProc = OpenProcess(SYNCHRONIZE, FALSE, pid)))
        NS_FAILED(explProc, "Cannot open explorer proces");

    if (!PostMessage(explWin, WM_QUIT, 0, 0))
        NS_FAILED(explProc, "Cannot send WM_QUIT to explorer window");

    switch (WaitForSingleObject(explProc, timeout))
    {
        case WAIT_OBJECT_0: break; /* OK */
        case WAIT_ABANDONED:
            NS_FAILED(explProc, "WaitForSingleObject() returned Abandoned (?)");
        case WAIT_TIMEOUT:
            if (timeout == IGNORE) break; /* OK as requested */
            StartExplorer(IGNORE); /* restart anyway or the user will have no shell */
            NS_FAILED(explProc, "Timeout while waiting for explorer process termination");
    }

    CloseHandle(explProc);
    return TRUE;
}

BOOL RestartExplorer(DWORD timeout)
{
    OutputDebugStringA("nsRE::RestartExplorer");
    QuitExplorer(timeout);
    return StartExplorer(timeout);
}
