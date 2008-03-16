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
#include <ctype.h>

action_t nsiParseAction(char *argument)
{
    action_t action = ACTION_INVALID;
    char *p = argument;
    while (*p) { *p = tolower((unsigned char) *p); p++; };

    if (!strcmp(argument, "start")) action = ACTION_START;
    else if(!strcmp(argument, "quit")) action = ACTION_QUIT;
    else if(!strcmp(argument, "restart")) action = ACTION_RESTART;

    return action;
}

BOOL nsiParseTimeout(char *argument, LPDWORD timeout)
{
    BOOL res = TRUE;
    char *p = argument;
    while (*p) { *p = tolower((unsigned char) *p); p++; };

    if (!strcmp(argument, "infinite"))
        *timeout = INFINITE;
    else if (!strcmp(argument, "ignore"))
        *timeout = IGNORE;
    else if ((argument[0] == '-') || !(*timeout = atoi(argument)))
        res = FALSE;
    return res;
}

/* RunDll32 */
void CALLBACK nsRE(NS_UNUSED HWND hwnd, NS_UNUSED HINSTANCE hinst, LPSTR lpszCmdLine, NS_UNUSED int nCmdShow)
{
    DWORD timeout = IGNORE;
    action_t action = ACTION_INVALID;
    BOOL result = FALSE;
    char *p = NULL;

    if ((p = strchr(lpszCmdLine, ' ')))
    {
        *p = 0; p++;
        if ((action = nsiParseAction(lpszCmdLine)) == ACTION_INVALID)
        {
            NS_SHOWERR("Invalid Action");
            return;
        }
    }

    if (!p)
    {
        NS_SHOWERR("Invalid Arguments");
        return;
    }

    if (!(*p) || !nsiParseTimeout(p, &timeout))
    {
        NS_SHOWERR("Invalid Timeout");
        return;
    }

    NS_DOACTION();
}

BOOL FakeStartupIsDone(void)
{
    OSVERSIONINFO osv;
    TOKEN_STATISTICS tst;
    DWORD osz;
    HANDLE hToken;
    HKEY hk;
    char sinfo[MAX_PATH] = "";

    osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osv);

    if (osv.dwPlatformId != VER_PLATFORM_WIN32_NT)
    {
        OutputDebugStringA("FakeStartupIsDone::No Need");
        return TRUE;
    }

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        OutputDebugStringA("FakeStartupIsDone::OpenProcessToken");
        return FALSE;
    }

    if (!GetTokenInformation(hToken, TokenStatistics, &tst, sizeof(TOKEN_STATISTICS), &osz))
    {
        CloseHandle(hToken);
        OutputDebugStringA("FakeStartupIsDone::GetTokenInformation");
        return FALSE;
    }

    CloseHandle(hToken);

    _snprintf(sinfo, MAX_PATH - 1, "%s\\%08x%08x", SESSIONINFOKEY, tst.AuthenticationId.HighPart, tst.AuthenticationId.LowPart);
    sinfo[MAX_PATH - 1] = 0;

    if (RegCreateKeyExA(HKEY_CURRENT_USER, sinfo, 0, NULL, REG_OPTION_VOLATILE, MAXIMUM_ALLOWED, NULL, &hk, NULL))
    {
        OutputDebugStringA("FakeStartupIsDone::RegCreateKeyExA SessionInfo");
        return FALSE;
    }

    if (RegCreateKeyExA(hk, "StartupHasBeenRun", 0, NULL, REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
    {
        OutputDebugStringA("FakeStartupIsDone::RegCreateKeyExA StartupHasBeenRun");
        RegCloseKey(hk);
        return FALSE;
    }

    RegCloseKey(hk);
    return TRUE;
}

BOOL StartExplorer(DWORD timeout)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    PVOID OldValue = NULL;
    BOOL redirOk = FALSE;
    OutputDebugStringA("nsRE::StartExplorer");

    if (FindWindowA(SHELLWND, NULL))
        NS_FAILED(NULL, "Explorer already started");

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

    if (!(explWin = FindWindowA(SHELLWND, NULL)))
        NS_FAILED(explProc, "Cannot find explorer window");

    GetWindowThreadProcessId(explWin, &pid);

    if (!(explProc = OpenProcess(SYNCHRONIZE, FALSE, pid)))
        NS_FAILED(explProc, "Cannot open explorer proces");

    if (!PostMessageA(explWin, WM_QUIT, 0, 0))
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
    return FakeStartupIsDone();
}

BOOL RestartExplorer(DWORD timeout)
{
    OutputDebugStringA("nsRE::RestartExplorer");
    QuitExplorer(timeout);
    return StartExplorer(timeout);
}
