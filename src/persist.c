#include "persist.h"
#include <wchar.h>

#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

int persist_install(const WCHAR *value_name, const WCHAR *command_line)
{
    HKEY hKey;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hKey);
    if (rc != ERROR_SUCCESS)
        return -1;

    DWORD cb = (DWORD)((wcslen(command_line) + 1) * sizeof(WCHAR));
    rc = RegSetValueExW(hKey, value_name, 0, REG_SZ, (const BYTE *)command_line, cb);
    RegCloseKey(hKey);

    return (rc == ERROR_SUCCESS) ? 0 : -1;
}

int persist_remove(const WCHAR *value_name)
{
    HKEY hKey;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hKey);
    if (rc != ERROR_SUCCESS)
        return -1;

    rc = RegDeleteValueW(hKey, value_name);
    RegCloseKey(hKey);

    return (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND) ? 0 : -1;
}

int persist_check(const WCHAR *value_name)
{
    HKEY hKey;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (rc != ERROR_SUCCESS)
        return 0;

    rc = RegQueryValueExW(hKey, value_name, NULL, NULL, NULL, NULL);
    RegCloseKey(hKey);

    return (rc == ERROR_SUCCESS) ? 1 : 0;
}
