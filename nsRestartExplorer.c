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

/* __debugbreak(); */

pW64NoRedirF pW64NoRedir = NULL;
pW64RevertF pW64Revert = NULL;

unsigned int g_stringsize = 0;
stack_t **g_stacktop = NULL;
char *g_variables = NULL;

BOOL nsiParseArguments(action_t *action, LPDWORD timeout)
{
    char *argument = NULL;
    BOOL res = FALSE;
    *timeout = IGNORE;
    *action = ACTION_INVALID;

    OutputDebugStringA("nsRE::nsiParseTimeout");

    /* Action */
    if (!(argument = malloc(g_stringsize + 1))) return FALSE;
    popstring(argument);
    argument[g_stringsize] = 0;
    *action = nsiParseAction(argument);
    free(argument);

    /* Timeout */
    if (!(argument = malloc(g_stringsize + 1))) return FALSE;
    popstring(argument);
    argument[g_stringsize] = 0;
    res = nsiParseTimeout(argument, timeout);
    free(argument);

    if (res && (*action != ACTION_INVALID)) return TRUE;
    return FALSE;
}

PLUGINFUNCTION(nsRestartExplorer)
{
    DWORD timeout = IGNORE;
    action_t action = ACTION_INVALID;
    BOOL result = FALSE;

    if (!nsiParseArguments(&action, &timeout))
    {
        pushstring("Invalid arguments");
        return;
    }

    NS_DOACTION();
    if (result) pushstring("OK");

} PLUGINFUNCTIONEND

/* Dynamic load of Wow64 FS Redirectors */
BOOL nsiLoadRedirectors(void)
{
    HMODULE hK32 = GetModuleHandleA("kernel32.dll");
    if (!hK32) return FALSE; /* Unlikely */
    pW64NoRedir = (pW64NoRedirF) GetProcAddress(hK32, "Wow64DisableWow64FsRedirection");
    pW64Revert = (pW64RevertF) GetProcAddress(hK32, "Wow64RevertWow64FsRedirection");
    return (pW64NoRedir && pW64Revert);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, NS_UNUSED LPVOID lpReserved)
{
	switch (reason)
	{
	    case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            OutputDebugStringA("nsRE::DllMain");
            nsiLoadRedirectors();
	    case DLL_THREAD_ATTACH:
	    case DLL_THREAD_DETACH:
	    case DLL_PROCESS_DETACH:
		    break;
	}
    return TRUE;
}
