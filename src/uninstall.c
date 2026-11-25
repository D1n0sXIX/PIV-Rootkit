#include "persist.h"
#include "config_reader.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#define PERSIST_NAME L"WindowsUpdate"

static void delete_prefixed(const WCHAR *dir, const WCHAR *prefix)
{
    WCHAR pattern[MAX_PATH];
    swprintf(pattern, MAX_PATH, L"%ls\\%ls*", dir, prefix);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        wprintf(L"    No hay ficheros con prefijo '%ls'\n", prefix);
        return;
    }

    do {
        WCHAR fullPath[MAX_PATH];
        swprintf(fullPath, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        if (DeleteFileW(fullPath))
            wprintf(L"    [-] Borrado: %ls\n", fd.cFileName);
        else
            wprintf(L"    [!] No se pudo borrar %ls (error %lu, puede estar en uso)\n",
                    fd.cFileName, GetLastError());
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

int wmain(void)
{
    WCHAR installDir[MAX_PATH];
    GetModuleFileNameW(NULL, installDir, MAX_PATH);
    WCHAR *slash = wcsrchr(installDir, L'\\');
    if (slash) *slash = L'\0';

    /* Leer prefijo del config.txt */
    WCHAR cfgPath[MAX_PATH];
    swprintf(cfgPath, MAX_PATH, L"%ls\\config.txt", installDir);

    PrefixTable table;
    prefix_table_init(&table);
    if (prefix_table_load(&table, cfgPath) != 0 || table.count == 0) {
        wprintf(L"[!] No se pudo leer config.txt o esta vacio\n");
        wprintf(L"    Ruta: %ls\n", cfgPath);
        return 1;
    }

    const WCHAR *prefix = table.prefixes[0];

    wprintf(L"=== PIV-Rootkit Uninstaller ===\n");
    wprintf(L"Directorio: %ls\n", installDir);
    wprintf(L"Prefijo:    %ls (leido de config.txt)\n\n", prefix);

    /* 1. Quitar persistencia */
    wprintf(L"[1] Eliminando clave Run '%ls'...\n", PERSIST_NAME);
    if (persist_check(PERSIST_NAME)) {
        if (persist_remove(PERSIST_NAME) == 0)
            wprintf(L"[+] Clave Run eliminada\n");
        else
            wprintf(L"[!] Error al eliminar la clave Run\n");
    } else {
        wprintf(L"    No existe, nada que hacer\n");
    }

    /* 2. Borrar ficheros con prefijo */
    wprintf(L"\n[2] Borrando ficheros con prefijo '%ls'...\n", prefix);
    delete_prefixed(installDir, prefix);

    wprintf(L"\n=== Desinstalacion completada ===\n");
    wprintf(L"NOTA: los hooks en procesos ya inyectados siguen activos\n");
    wprintf(L"      hasta que esos procesos se cierren o se reinicie.\n");
    wprintf(L"      La persistencia ya no reinyectara al iniciar sesion.\n");

    return 0;
}
