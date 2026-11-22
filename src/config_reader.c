#include "config_reader.h"
#include <string.h>
#include <wchar.h>

#define MAX_FILE_SIZE 4096

void prefix_table_init(PrefixTable *table) {
    ZeroMemory(table, sizeof(PrefixTable));
}

int prefix_table_load(PrefixTable *table, const WCHAR *filepath) {
    HANDLE hFile = CreateFileW(filepath, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }

    char buffer[MAX_FILE_SIZE];
    DWORD bytesRead;
    if (!ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        CloseHandle(hFile);
        return -1;
    }

    buffer[bytesRead] = '\0';
    CloseHandle(hFile);

    char *line = strtok(buffer, "\r\n");
    while (line != NULL && table->count < MAX_PREFIXES) {
        if (line[0] != '\0') {
            MultiByteToWideChar(CP_UTF8, 0, line, -1,
                                table->prefixes[table->count], MAX_PREFIX_LEN);
            table->count++;
        }
        line = strtok(NULL, "\r\n");
    }

    return 0;
}

int prefix_matches(const PrefixTable *table, const WCHAR *name) {
    for (int i = 0; i < table->count; i++) {
        size_t len = wcslen(table->prefixes[i]);
        if (wcsncmp(name, table->prefixes[i], len) == 0) {
            return 1;
        }
    }
    return 0;
}
