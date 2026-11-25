#ifndef PERSIST_H
#define PERSIST_H

#include <windows.h>

/* Escribe una clave Run en HKCU para ejecutar command_line al iniciar sesion.
 * value_name: nombre del valor en la clave Run (p.ej. "WindowsUpdate").
 * command_line: ruta completa del ejecutable + argumentos.
 * Devuelve 0 si OK, -1 si falla. */
int persist_install(const WCHAR *value_name, const WCHAR *command_line);

/* Elimina el valor value_name de la clave Run de HKCU.
 * Devuelve 0 si OK o si no existia, -1 si falla. */
int persist_remove(const WCHAR *value_name);

/* Comprueba si value_name existe en la clave Run de HKCU.
 * Devuelve 1 si existe, 0 si no. */
int persist_check(const WCHAR *value_name);

#endif
