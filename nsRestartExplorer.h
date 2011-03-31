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

#ifndef _NS_RESTART_EXPLORER_H
#define _NS_RESTART_EXPLORER_H

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>

#define SHELL       _T("\\explorer.exe")
#define SHELLWND    _T("Progman")

#define SESSIONINFOKEY _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SessionInfo")

#ifdef _DEBUG
#define NS_SHOWERR(x) OutputDebugString(_T("nsRE:")x)
#else
#define NS_SHOWERR(x) MessageBox(NULL, x, _T("nsRestartExplorer Error"), MB_ICONERROR | MB_OK)
#undef OutputDebugString
#define OutputDebugString(x) {}
#endif

#if !defined(__cplusplus) && !defined(__GNUC__)
#define inline _inline
#endif

#ifdef __GNUC__
#define NS_UNUSED __attribute__((unused))
#else
#define NS_UNUSED
#endif

typedef enum
{
    ACTION_START = 0,
    ACTION_QUIT,
    ACTION_RESTART,
    ACTION_INVALID
} action_t;

typedef struct _stack_t {
  struct _stack_t *next;
  TCHAR text[1];
} stack_t;

#define EXDLL_INIT()            \
{                               \
    g_stringsize = string_size; \
    g_stacktop = stacktop;      \
    g_variables = variables;    \
}

extern unsigned int g_stringsize;
extern stack_t **g_stacktop;
extern TCHAR *g_variables;

extern BOOL StartExplorer(DWORD timeout, BOOL kill);
extern BOOL QuitExplorer(DWORD timeout, BOOL kill);
extern BOOL RestartExplorer(DWORD timeout, BOOL kill);

extern action_t nsiParseAction(TCHAR *argument);
extern BOOL nsiParseTimeout(TCHAR *argument, LPDWORD timeout);

static inline int popstring(TCHAR *str)
{
    stack_t *th;
    if (!g_stacktop || !*g_stacktop) return 1;
    th = (*g_stacktop);
    lstrcpy(str, th->text);
    *g_stacktop = th->next;
    GlobalFree((HGLOBAL) th);
    return 0;
}

static inline void pushstring(const TCHAR *str)
{
    stack_t *th;
    if (!g_stacktop) return;
    th = (stack_t *) GlobalAlloc(GPTR, sizeof(stack_t) + g_stringsize);
    lstrcpyn(th->text, str, g_stringsize);
    th->next = *g_stacktop;
    *g_stacktop = th;
}

#define PLUGINFUNCTION(name) \
    void name(NS_UNUSED HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop) { \
        g_stringsize = string_size; \
        g_stacktop = stacktop; \
        g_variables = variables;
#define PLUGINFUNCTIONEND }

#define inNSIS() (g_stringsize && g_stacktop && g_variables)

#define NS_FAILED(handle, text)                             \
do                                                          \
{                                                           \
    if (inNSIS())                                           \
        pushstring(_T("ERROR ")text);                       \
    else                                                    \
        OutputDebugString(_T("nsRE::!!") text _T("!!"));    \
    if (handle && (handle != INVALID_HANDLE_VALUE))         \
        CloseHandle(handle);                                \
    return FALSE;                                           \
} while (0)

#define NS_DOACTION() \
    switch (action) \
    { \
        case ACTION_START: \
            result = StartExplorer(timeout, kill); \
            break; \
        case ACTION_QUIT: \
            result = QuitExplorer(timeout, kill); \
            break; \
        case ACTION_RESTART: \
            result = RestartExplorer(timeout, kill); \
            break; \
        default: \
            break; \
    }
#endif /* _NS_RESTART_EXPLORER_H */
