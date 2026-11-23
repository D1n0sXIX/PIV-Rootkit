#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int wmain(int argc, WCHAR *argv[])
{
    const WCHAR *dllPath = L"hook.dll";
    if (argc > 1)
        dllPath = argv[1];

    wprintf(L"=== ANTES del hook ===\n");
    HANDLE snap1 = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe1;
    pe1.dwSize = sizeof(pe1);
    if (Process32FirstW(snap1, &pe1)) {
        do {
            if (pe1.szExeFile[0] == L'$')
                wprintf(L"  PID %5lu  %ls\n", pe1.th32ProcessID, pe1.szExeFile);
        } while (Process32NextW(snap1, &pe1));
    }
    CloseHandle(snap1);

    wprintf(L"\nCargando DLL: %ls\n", dllPath);
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll) {
        wprintf(L"Error: LoadLibrary fallo (code %lu)\n", GetLastError());
        return 1;
    }
    wprintf(L"DLL cargada OK.\n\n");

    wprintf(L"=== DESPUES del hook ===\n");
    HANDLE snap2 = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe2;
    pe2.dwSize = sizeof(pe2);
    if (Process32FirstW(snap2, &pe2)) {
        do {
            if (pe2.szExeFile[0] == L'$')
                wprintf(L"  PID %5lu  %ls\n", pe2.th32ProcessID, pe2.szExeFile);
        } while (Process32NextW(snap2, &pe2));
    }
    CloseHandle(snap2);

    wprintf(L"\nDescargando DLL...\n");
    FreeLibrary(hDll);
    wprintf(L"Hecho.\n");
    return 0;
}
