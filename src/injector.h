#ifndef INJECTOR_H
#define INJECTOR_H

#include <windows.h>

int enable_debug_privilege(void);
int inject_dll(DWORD pid, const WCHAR *dllPath);
int inject_all(const WCHAR *dllPath);

#endif
