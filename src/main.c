#include "config_reader.h"
#include "process_scanner.h"
#include <stdio.h>

int wmain(int argc, WCHAR *argv[]) {
    const WCHAR *configPath = L"config.txt";
    if (argc > 1) {
        configPath = argv[1];
    }

    PrefixTable table;
    prefix_table_init(&table);

    if (prefix_table_load(&table, configPath) != 0) {
        wprintf(L"Error: no se pudo abrir '%ls'\n", configPath);
        return 1;
    }

    wprintf(L"Prefijos cargados: %d\n", table.count);
    for (int i = 0; i < table.count; i++) {
        wprintf(L"  [%d] '%ls'\n", i, table.prefixes[i]);
    }

    wprintf(L"\nEscaneando procesos...\n\n");
    int matches = scan_and_print_matches(&table);

    if (matches < 0) {
        wprintf(L"Error: no se pudo crear el snapshot de procesos\n");
        return 1;
    }

    wprintf(L"\nTotal: %d proceso(s) coinciden con los prefijos\n", matches);
    return 0;
}
