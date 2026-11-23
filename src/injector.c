#include "injector.h"
#include <tlhelp32.h>
#include <wchar.h>
#include <stdio.h>

static const WCHAR *skip_list[] = {
    L"System", L"smss.exe", L"csrss.exe", L"wininit.exe",
    L"winlogon.exe", L"services.exe", L"lsass.exe",
    L"svchost.exe", L"dwm.exe"
};
static const int skip_count = sizeof(skip_list) / sizeof(skip_list[0]);

static int is_skipped(const WCHAR *name)
{
    for (int i = 0; i < skip_count; i++) {
        if (_wcsicmp(name, skip_list[i]) == 0)
            return 1;
    }
    return 0;
}

int enable_debug_privilege(void)
{
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return -1;

    if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return -1;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL)) {
        CloseHandle(hToken);
        return -1;
    }

    CloseHandle(hToken);
    return 0;
}

int inject_dll(DWORD pid, const WCHAR *dllPath)
{
    SIZE_T pathSize = (wcslen(dllPath) + 1) * sizeof(WCHAR);

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProcess)
        return -1;

    LPVOID remoteBuf = VirtualAllocEx(hProcess, NULL, pathSize,
                                      MEM_COMMIT | MEM_RESERVE,
                                      PAGE_READWRITE);
    if (!remoteBuf) {
        CloseHandle(hProcess);
        return -1;
    }

    if (!WriteProcessMemory(hProcess, remoteBuf, dllPath, pathSize, NULL)) {
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return -1;
    }

    FARPROC loadLibAddr = GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    if (!loadLibAddr) {
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return -1;
    }

    HANDLE hThread = CreateRemoteThread(
        hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)loadLibAddr, remoteBuf, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return -1;
    }

    WaitForSingleObject(hThread, 5000);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteBuf, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return 0;
}

int inject_all(const WCHAR *dllPath)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return -1;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(hSnapshot, &pe)) {
        CloseHandle(hSnapshot);
        return -1;
    }

    DWORD myPid = GetCurrentProcessId();
    int count = 0;

    do {
        if (pe.th32ProcessID == 0 || pe.th32ProcessID == myPid)
            continue;
        if (is_skipped(pe.szExeFile))
            continue;
        if (inject_dll(pe.th32ProcessID, dllPath) == 0) {
            wprintf(L"  [OK] PID %5lu  %ls\n",
                    pe.th32ProcessID, pe.szExeFile);
            count++;
        }
    } while (Process32NextW(hSnapshot, &pe));

    CloseHandle(hSnapshot);
    return count;
}
