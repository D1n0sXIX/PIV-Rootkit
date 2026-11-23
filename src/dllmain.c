#include "hook.h"
#include "config_reader.h"
#include <windows.h>
#include <wchar.h>

static PrefixTable g_prefixes;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH: {
        WCHAR dllPath[MAX_PATH];
        GetModuleFileNameW(hinstDLL, dllPath, MAX_PATH);
        WCHAR *lastSlash = wcsrchr(dllPath, L'\\');
        if (lastSlash)
            wcscpy(lastSlash + 1, L"config.txt");
        else
            wcscpy(dllPath, L"config.txt");

        prefix_table_init(&g_prefixes);
        if (prefix_table_load(&g_prefixes, dllPath) != 0)
            return FALSE;
        if (hook_install(&g_prefixes) != 0)
            return FALSE;
        break;
    }
    case DLL_PROCESS_DETACH:
        hook_remove();
        break;
    }
    return TRUE;
}
