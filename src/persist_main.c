#include "persist.h"
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, WCHAR *argv[])
{
    if (argc < 3) {
        wprintf(L"Uso:\n");
        wprintf(L"  persist.exe install <nombre> <comando>\n");
        wprintf(L"  persist.exe remove  <nombre>\n");
        wprintf(L"  persist.exe check   <nombre>\n");
        return 1;
    }

    const WCHAR *action = argv[1];
    const WCHAR *name   = argv[2];

    if (wcscmp(action, L"install") == 0) {
        if (argc < 4) {
            wprintf(L"Falta el comando a ejecutar\n");
            return 1;
        }
        if (persist_install(name, argv[3]) == 0)
            wprintf(L"Clave Run '%ls' instalada.\n", name);
        else
            wprintf(L"Error al instalar la clave Run.\n");
    } else if (wcscmp(action, L"remove") == 0) {
        if (persist_remove(name) == 0)
            wprintf(L"Clave Run '%ls' eliminada.\n", name);
        else
            wprintf(L"Error al eliminar la clave Run.\n");
    } else if (wcscmp(action, L"check") == 0) {
        if (persist_check(name))
            wprintf(L"Clave Run '%ls' EXISTE.\n", name);
        else
            wprintf(L"Clave Run '%ls' no existe.\n", name);
    } else {
        wprintf(L"Accion desconocida: %ls\n", action);
        return 1;
    }

    return 0;
}
