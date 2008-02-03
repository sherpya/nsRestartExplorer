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

static BOOL nsiParseTimeout(LPDWORD timeout)
{
    BOOL res = TRUE;
    char *argument = NULL;

    OutputDebugStringA("nsRE::nsiParseTimeout");

    if (!(argument = malloc(g_stringsize + 1)))
        return FALSE; /* very weird if malloc() fails ;) */

    popstring(argument);
    argument[g_stringsize] = 0;

    if (!strcmp(argument, "INFINITE"))
        *timeout = INFINITE;
    else if (!strcmp(argument, "IGNORE"))
        *timeout = IGNORE;
    else if ((argument[0] == '-') || !(*timeout = atoi(argument)))
        res = FALSE;

    free(argument);
    return res;
}

#define nsDECLARE_FUNC(name)                                \
    PLUGINFUNCTION(ns##name)                                \
    {                                                       \
        DWORD timeout = INFINITE;                           \
        OutputDebugStringA("nsRE::ns"#name);                \
        if (nsiParseTimeout(&timeout) && name(timeout))     \
            pushstring("OK");                               \
    } PLUGINFUNCTIONEND

nsDECLARE_FUNC(StartExplorer)
nsDECLARE_FUNC(QuitExplorer)
nsDECLARE_FUNC(RestartExplorer)

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
