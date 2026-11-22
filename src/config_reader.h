#ifndef CONFIG_READER_H
#define CONFIG_READER_H

#include <windows.h>

#define MAX_PREFIXES 64
#define MAX_PREFIX_LEN 256

typedef struct {
    WCHAR prefixes[MAX_PREFIXES][MAX_PREFIX_LEN];
    int count;
} PrefixTable;

/* Inicializa la tabla a cero. */
void prefix_table_init(PrefixTable *table);

/* Lee prefijos de un fichero de texto (uno por linea, UTF-8 o ANSI).
 * Devuelve 0 si OK, -1 si no puede abrir el fichero. */
int prefix_table_load(PrefixTable *table, const WCHAR *filepath);

/* Devuelve 1 si name empieza por alguno de los prefijos cargados, 0 si no. */
int prefix_matches(const PrefixTable *table, const WCHAR *name);

#endif
