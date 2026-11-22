#ifndef PROCESS_SCANNER_H
#define PROCESS_SCANNER_H

#include <windows.h>
#include "config_reader.h"

/* Enumera todos los procesos del sistema con CreateToolhelp32Snapshot.
 * Para cada proceso cuyo nombre (szExeFile) empiece por un prefijo
 * de la tabla, imprime PID y nombre por stdout.
 * Devuelve el numero de procesos que coincidieron. */
int scan_and_print_matches(const PrefixTable *table);

#endif
