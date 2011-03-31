/*
 * NSIS Plugin to gracefully restart explorer
 *
 * Copyright (c) 2008-2010 Gianluigi Tiesi <sherpya@netfarm.it>
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

typedef struct _nsExpData
{
    HWND explWin;
    DWORD pid;
} nsExpData;

action_t nsiParseAction(TCHAR *argument)
{
    action_t action = ACTION_INVALID;

    if (!_tcsicmp(argument, _T("start"))) action = ACTION_START;
    else if(!_tcsicmp(argument, _T("quit"))) action = ACTION_QUIT;
    else if(!_tcsicmp(argument, _T("restart"))) action = ACTION_RESTART;

    return action;
}

BOOL nsiParseTimeout(TCHAR *argument, LPDWORD timeout)
{
    BOOL res = TRUE;

    if (!_tcsicmp(argument, _T("infinite")))
        *timeout = INFINITE;
    else if (!_tcsicmp(argument, _T("ignore")))
        *timeout = IGNORE;
    else if ((argument[0] == _T('-')) || !(*timeout = _ttoi(argument)))
        res = FALSE;
    return res;
}

/* RunDll32 */
void CALLBACK nsRE(NS_UNUSED HWND hwnd, NS_UNUSED HINSTANCE hinst, LPTSTR lpszCmdLine, NS_UNUSED int nCmdShow)
{
    DWORD timeout = IGNORE;
    action_t action = ACTION_INVALID;
    BOOL result = FALSE, kill = FALSE;
    TCHAR *p = NULL, *e = NULL;

    if ((p = _tcschr(lpszCmdLine, _T(' '))))
    {
        *p = 0; p++;
        if ((action = nsiParseAction(lpszCmdLine)) == ACTION_INVALID)
        {
            NS_SHOWERR(_T("Invalid Action"));
            return;
        }
    }

    if (!p)
    {
        NS_SHOWERR(_T("Invalid Arguments"));
        return;
    }

    if ((e = _tcschr(p, _T(' '))))
    {
        *e = 0;
        e++;
    }

    if (!nsiParseTimeout(p, &timeout))
    {
        NS_SHOWERR(_T("Invalid Timeout"));
        return;
    }

    if (e && !_tcsnicmp(e, _T("kill"), 4))
        kill = TRUE;

    NS_DOACTION();
}

BOOL FakeStartupIsDone(void)
{
    OSVERSIONINFO osv;
    TOKEN_STATISTICS tst;
    DWORD osz;
    HANDLE hToken;
    HKEY hk;
    TCHAR sinfo[MAX_PATH];

    osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osv);

    if (osv.dwPlatformId != VER_PLATFORM_WIN32_NT)
    {
        OutputDebugString(_T("FakeStartupIsDone::No Need"));
        return TRUE;
    }

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        OutputDebugString(_T("FakeStartupIsDone::OpenProcessToken"));
        return FALSE;
    }

    if (!GetTokenInformation(hToken, TokenStatistics, &tst, sizeof(TOKEN_STATISTICS), &osz))
    {
        CloseHandle(hToken);
        OutputDebugString(_T("FakeStartupIsDone::GetTokenInformation"));
        return FALSE;
    }

    CloseHandle(hToken);

    _sntprintf(sinfo, MAX_PATH - 1, _T("%s\\%08x%08x"), SESSIONINFOKEY, tst.AuthenticationId.HighPart, tst.AuthenticationId.LowPart);
    sinfo[MAX_PATH - 1] = 0;

    if (RegCreateKeyEx(HKEY_CURRENT_USER, sinfo, 0, NULL, REG_OPTION_VOLATILE, MAXIMUM_ALLOWED, NULL, &hk, NULL))
    {
        OutputDebugString(_T("FakeStartupIsDone::RegCreateKeyExA SessionInfo"));
        return FALSE;
    }

    if (RegCreateKeyEx(hk, _T("StartupHasBeenRun"), 0, NULL, REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hk, NULL))
    {
        OutputDebugString(_T("FakeStartupIsDone::RegCreateKeyExA StartupHasBeenRun"));
        RegCloseKey(hk);
        return FALSE;
    }

    RegCloseKey(hk);
    return TRUE;
}

BOOL StartExplorer(DWORD timeout, NS_UNUSED BOOL kill)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    TCHAR shellpath[MAX_PATH];

    OutputDebugString(_T("nsRE::StartExplorer"));

    if (FindWindow(SHELLWND, NULL))
        NS_FAILED(NULL, _T("Explorer already running"));

    GetWindowsDirectory(shellpath, MAX_PATH - 1);
    shellpath[MAX_PATH - 1] = 0;
    _tcsncat(shellpath, SHELL, MAX_PATH - 1);
    shellpath[MAX_PATH - 1] = 0;

    FakeStartupIsDone();

    memset(&pi, 0, sizeof(PROCESS_INFORMATION));
    memset(&si, 0, sizeof(STARTUPINFO));

    si.cb = sizeof(STARTUPINFO);

    if(!CreateProcess(NULL,     /* No module name (use command line) */
        shellpath,              /* Command line */
        NULL,                   /* Process handle not inheritable */
        NULL,                   /* Thread handle not inheritable */
        FALSE,                  /* Set handle inheritance to FALSE */
        0,                      /* No creation flags */
        NULL,                   /* Use parent's environment block */
        NULL,                   /* Use parent's starting directory */
        &si,                    /* Pointer to STARTUPINFO structure */
        &pi))                   /* Pointer to PROCESS_INFORMATION structure */
        NS_FAILED(NULL, _T("Cannot spawn explorer process"));

    switch (WaitForInputIdle(pi.hProcess, timeout))
    {
        case 0            : break; /* OK */
        case WAIT_TIMEOUT :
            if (timeout == IGNORE) break; /* OK as requested */
            NS_FAILED(pi.hProcess, _T("Timeout while waiting for explorer process"));
        case WAIT_FAILED  : NS_FAILED(pi.hProcess, _T("Error while waiting for explorer process"));
        default           : NS_FAILED(pi.hProcess, _T("This should not be reached"));
    }

    return TRUE;
}

BOOL CALLBACK CloseExplorerWindows(HWND hwnd, LPARAM lParam)
{
    nsExpData *exData = (nsExpData *) lParam;
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == exData->pid)
    {
#ifdef _DEBUG
        TCHAR buff[1024], title[1024];
        GetWindowText(hwnd, title, sizeof(title) - 1);
        _sntprintf(buff, sizeof(buff) - 1, _T("nsRE::CallBack closing window %s (0x%p)"), title, hwnd);
        OutputDebugString(buff);
#endif
        PostMessage(hwnd, WM_QUIT, 0, 0);
    }

    return TRUE;
}

BOOL QuitExplorer(DWORD timeout, NS_UNUSED BOOL kill)
{
    HANDLE explProc = NULL;
    nsExpData exData;

    OutputDebugString(_T("nsRE::QuitExplorer"));

    if (!(exData.explWin = FindWindow(SHELLWND, NULL)))
        NS_FAILED(explProc, _T("Cannot find explorer window"));

    GetWindowThreadProcessId(exData.explWin, &exData.pid);

    if (!(explProc = OpenProcess(SYNCHRONIZE, FALSE, exData.pid)))
        NS_FAILED(explProc, _T("Cannot open explorer proces"));

    EnumWindows(CloseExplorerWindows, (LPARAM) &exData);

    switch (WaitForSingleObject(explProc, timeout))
    {
        case WAIT_OBJECT_0: break; /* OK */
        case WAIT_ABANDONED:
            NS_FAILED(explProc, _T("WaitForSingleObject() returned Abandoned (?)"));
        case WAIT_TIMEOUT:
            if (timeout == IGNORE) break; /* OK as requested */
            if (kill)
            {
                OutputDebugString(_T("nsRE::QuitExplorer Terminating explorer"));
                TerminateProcess(explProc, 0); /* Kill the process if requested */
            }
            StartExplorer(IGNORE, FALSE); /* restart anyway or the user will have no shell */
            if (kill)
                NS_FAILED(explProc, _T("Process killed due to timeout"));
            else
                NS_FAILED(explProc, _T("Timeout while waiting for explorer process termination"));
    }

    CloseHandle(explProc);
    return TRUE;
}

BOOL RestartExplorer(DWORD timeout, BOOL kill)
{
    OutputDebugString(_T("nsRE::RestartExplorer"));
    return (QuitExplorer(timeout, kill) && StartExplorer(timeout, kill));
}
