#include "injector.h"
#include <stdio.h>
#include <stdlib.h>

int wmain(int argc, WCHAR *argv[])
{
    if (argc < 2) {
        wprintf(L"Uso:\n");
        wprintf(L"  injector.exe <dllPath>           Inyecta en todos los procesos\n");
        wprintf(L"  injector.exe <dllPath> <PID>      Inyecta en un proceso\n");
        return 1;
    }

    const WCHAR *dllPath = argv[1];

    wprintf(L"Activando SeDebugPrivilege...\n");
    if (enable_debug_privilege() != 0) {
        wprintf(L"[WARN] No se pudo activar SeDebugPrivilege (puede fallar en procesos elevados)\n");
    } else {
        wprintf(L"SeDebugPrivilege activado.\n");
    }

    if (argc >= 3) {
        DWORD pid = (DWORD)wcstoul(argv[2], NULL, 10);
        wprintf(L"Inyectando %ls en PID %lu...\n", dllPath, pid);
        if (inject_dll(pid, dllPath) == 0) {
            wprintf(L"Inyeccion exitosa.\n");
        } else {
            wprintf(L"Error: no se pudo inyectar (code %lu)\n", GetLastError());
            return 1;
        }
    } else {
        wprintf(L"Inyectando %ls en todos los procesos...\n\n", dllPath);
        int count = inject_all(dllPath);
        wprintf(L"\nTotal: %d proceso(s) inyectados.\n", count);
    }

    return 0;
}
