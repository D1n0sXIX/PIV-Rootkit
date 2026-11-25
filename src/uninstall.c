#include "persist.h"
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

int wmain(int argc, WCHAR *argv[])
{
    WCHAR installDir[MAX_PATH];
    GetModuleFileNameW(NULL, installDir, MAX_PATH);
    WCHAR *slash = wcsrchr(installDir, L'\\');
    if (slash) *slash = L'\0';

    WCHAR prefix[64] = L"$rk";
    if (argc >= 2) wcscpy(prefix, argv[1]);

    wprintf(L"=== PIV-Rootkit Uninstaller ===\n");
    wprintf(L"Directorio: %ls\n", installDir);
    wprintf(L"Prefijo:    %ls\n\n", prefix);

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

    /* 3. Aviso: los hooks siguen activos hasta reiniciar */
    wprintf(L"\n=== Desinstalacion completada ===\n");
    wprintf(L"NOTA: los hooks en procesos ya inyectados siguen activos\n");
    wprintf(L"      hasta que esos procesos se cierren o se reinicie el sistema.\n");
    wprintf(L"      La persistencia ya no reinyectara al iniciar sesion.\n");

    return 0;
}
