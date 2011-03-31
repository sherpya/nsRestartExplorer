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

unsigned int g_stringsize = 0;
stack_t **g_stacktop = NULL;
TCHAR *g_variables = NULL;

BOOL nsiParseArguments(action_t *action, LPDWORD timeout, LPBOOL kill)
{
    size_t allocsize = (g_stringsize + 1) * sizeof(TCHAR);
    TCHAR *argument = NULL;
    BOOL res = FALSE;
    *timeout = IGNORE;
    *action = ACTION_INVALID;
    *kill = FALSE;
    

    OutputDebugString(_T("nsRE::nsiParseTimeout"));

    /* Action */
    if (!(argument = malloc(allocsize)))
        return FALSE;

    if (popstring(argument))
    {
        free(argument);
        return FALSE;
    }
    argument[g_stringsize] = 0;
    *action = nsiParseAction(argument);
    free(argument);

    /* Timeout */
    if (!(argument = malloc(allocsize)))
        return FALSE;

    if (popstring(argument))
    {
        free(argument);
        return FALSE;
    }
    res = nsiParseTimeout(argument, timeout);
    free(argument);

    /* KiLL */
    if (!(argument = malloc(allocsize)))
        return FALSE;

    if (!popstring(argument))
    {
        popstring(argument);
        argument[g_stringsize] = 0;
        *kill = !_tcsnicmp(argument, _T("kill"), 4);
    }
    free(argument);

    if (res && (*action != ACTION_INVALID)) return TRUE;
    return FALSE;
}

PLUGINFUNCTION(nsRestartExplorer)
{
    DWORD timeout = IGNORE;
    action_t action = ACTION_INVALID;
    BOOL kill = FALSE;
    BOOL result = FALSE;

    if (!nsiParseArguments(&action, &timeout, &kill))
    {
        pushstring(_T("Invalid arguments"));
        return;
    }

    NS_DOACTION();
    if (result) pushstring(_T("OK"));

} PLUGINFUNCTIONEND

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, NS_UNUSED LPVOID lpReserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            OutputDebugString(_T("nsRE::DllMain"));
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
