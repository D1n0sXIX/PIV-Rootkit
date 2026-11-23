#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int wmain(void)
{
    wprintf(L"PID de este proceso: %lu\n", GetCurrentProcessId());
    wprintf(L"Pulsa Enter DESPUES de inyectar hook.dll en este PID...\n");
    getwchar();

    wprintf(L"\nProcesos con prefijo $:\n");
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    int count = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.szExeFile[0] == L'$') {
                wprintf(L"  PID %5lu  %ls\n",
                        pe.th32ProcessID, pe.szExeFile);
                count++;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    if (count == 0)
        wprintf(L"  (ninguno visible — el hook funciona)\n");
    else
        wprintf(L"\n  Total: %d proceso(s) con prefijo $\n", count);

    wprintf(L"\nPulsa Enter para salir...\n");
    getwchar();
    return 0;
}
