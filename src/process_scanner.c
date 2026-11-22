#include "process_scanner.h"
#include <tlhelp32.h>
#include <stdio.h>

int scan_and_print_matches(const PrefixTable *table) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return -1;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return -1;
    }

    int matchCount = 0;
    do {
        if (prefix_matches(table, pe32.szExeFile)) {
            wprintf(L"[MATCH] PID: %5lu  Name: %ls\n",
                    pe32.th32ProcessID, pe32.szExeFile);
            matchCount++;
        }
    } while (Process32NextW(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return matchCount;
}
