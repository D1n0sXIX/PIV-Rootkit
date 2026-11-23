# Registro de uso de IA

Registro detallado del uso de IA en cada fase del desarrollo, para la documentación de la memoria final (punto 3).

## Modelos utilizados

| Modelo | Rol | Hardware | Parámetros |
|---|---|---|---|
| `huihui_ai/qwen3.5-abliterated:9b` | Arquitecto (diseño) | RX 580 8 GB, Ollama + Vulkan | think=false, ctx 8192, num_predict 2048 |
| `huihui_ai/qwen2.5-coder-abliterate:7b` | Codificador | RX 580 8 GB, Ollama + Vulkan | think=false, ctx 8192, num_predict 2048 |
| Claude Opus 4.6 | Revisor técnico | Remoto (Anthropic) | — |

## Fases

### Fase 1 — config_reader + process_scanner

**Fecha**: 2026-11-22

**Paso 1 — Diseño (Qwen 3.5, modelo `llm-cpp`)**
- Prompt: "Diseña las cabeceras .h de config_reader y process_scanner" con especificaciones de cada módulo
- Resultado: dos cabeceras con structs y prototipos. Problemas: PROCESSENTRY32 sin variante W, función get_process_name innecesaria, SIZE_T para un int, struct ProcessEntry con campo index inútil
- Tiempo: ~90 s

**Revisión de Claude**: se reescribieron las cabeceras simplificando la interfaz

**Paso 2 — Implementación (Qwen 2.5, modelo `llm-cpp-fast`)**
- Prompt por módulo: cabecera corregida + requisitos (CreateFileW/ReadFile, arrays estáticos, sin C++)
- Resultado config_reader.c: funcional, 2 bugs — (1) falta include propio, (2) strtok("\n") no quita \r (bug crítico en Windows: los prefijos llevarían \r y nunca matchearían)
- Resultado process_scanner.c: lógica correcta, 2 omisiones — falta include propio y stdio.h
- Tiempo: ~40 s cada módulo

**Correcciones de Claude**:
1. Añadidos `#include "config_reader.h"` y `#include "process_scanner.h"`
2. `strtok(buffer, "\n")` → `strtok(buffer, "\r\n")` (fix crítico para CRLF de Windows)
3. `CP_ACP` → `CP_UTF8` en MultiByteToWideChar
4. Añadido `#include <stdio.h>` para wprintf
5. main.c escrito directamente por Claude (punto de entrada trivial)

**Valoración**: el 2.5 produce código estructuralmente correcto pero con omisiones que impiden la compilación y un bug de compatibilidad Windows que haría que el programa no funcionase. Sin revisión, la Fase 1 no compilaría ni funcionaría.

**Compilación y prueba (2026-11-23)**
- Compilado con `x86_64-w64-mingw32-gcc -Wall -municode` desde Kali (PC-Personal), ya que la VM del lab no tiene compilador. Ficheros pasados a la VM vía carpeta compartida de VirtualBox (Guest Additions)
- Primer intento de prueba con `notepad.exe` copiado como `$rknotepad.exe` falló: notepad no arranca fuera de System32 (le faltan dependencias `.mui`). Este mismo problema afectó a la prueba de r77 del 2026-11-17 con `$77calc.exe`
- Prueba exitosa con `ping.exe` copiado como `$rkping.exe`: el scanner detectó correctamente el proceso (PID 4688, nombre `$rkping.exe`)
- **Fase 1 completada**: config_reader carga los 3 prefijos del `.txt` y process_scanner los matchea contra la lista de procesos del sistema

### Fase 2 — hook DLL (NtQuerySystemInformation)

**Fecha**: 2026-11-23

**Paso 1 — Diseño (Qwen 3.5, modelo `llm-cpp`)**
- Prompt: diseñar cabeceras ntdefs.h (structs NT para x64) y hook.h (API de inline hook)
- Resultado: completamente inutilizable. SYSTEM_PROCESS_INFORMATION con cientos de campos `ULONG Reserved` falsos, offsets incorrectos en los comentarios (dice NextEntryOffset = 2 bytes, es ULONG = 4), incluye `winternl.h` que entraría en conflicto, y el output se cortó por el límite de tokens (2048). No se puede aprovechar nada
- Tiempo: ~90 s

**Revisión de Claude**: se reescribieron ambas cabeceras completamente. ntdefs.h con la estructura correcta de SYSTEM_PROCESS_INFORMATION para x64 (offsets verificados: ImageName en 0x38), typedef de NtQuerySystemInformation_t, y prefijo MY_ en los structs para evitar conflictos. hook.h reducido a dos funciones (hook_install, hook_remove)

**Paso 2 — Implementación (Qwen 2.5, modelo `llm-cpp-fast`)**
- Prompt: implementar hook.c con las cabeceras corregidas, especificación detallada del mecanismo de inline hook (12 bytes: mov rax + jmp rax), trampoline, y filtrado
- Resultado: 10 bugs, el peor registro hasta ahora:
  1. `extern "C"` — C++, no C
  2. `#include <winternl.h>` — conflicto de tipos
  3. Trampoline escribe punteros como datos, no instrucciones de salto
  4. Hook install escribe un puntero en vez de un JMP
  5. VirtualProtect guarda oldProtect en `&state.original`, destruyendo el puntero a la función
  6. Llama a `state.original` (ya parcheada) en vez de `state.trampoline` — bucle infinito
  7. Loop `while (NextEntryOffset != 0)` no procesa la última entrada
  8. Rama muerta (NextEntryOffset == 0 dentro de while que ya exige != 0)
  9. VirtualFree con size != 0 y MEM_RELEASE (debe ser 0)
  10. Falta `#include <string.h>` para memcpy
- Tiempo: ~45 s

**Correcciones de Claude**: se reescribió hook.c completamente. Cambios principales:
1. Función `write_abs_jmp` que escribe los 12 bytes correctos (0x48 0xB8 [addr] 0xFF 0xE0)
2. Trampoline: 16 bytes salvados + JMP absoluto de vuelta a original+16
3. VirtualProtect con variable DWORD separada para oldProtect
4. Hook function llama a g_trampoline (no a g_original)
5. Bucle for(;;) con break al final cuando NextEntryOffset == 0
6. dllmain.c y test_hook.c escritos directamente por Claude

**Valoración**: la calidad del 2.5 se degrada significativamente con tareas complejas. En la Fase 1 (funciones simples de Win32) producía código estructuralmente correcto con omisiones menores. Aquí, con inline hooking x64, el código no es funcional en absoluto — ni la idea del inline hook (escribe datos, no instrucciones), ni la protección de memoria, ni el flujo de llamadas son correctos. El 3.5 como arquitecto tampoco aporta: su struct de 150+ campos Reserved demuestra que no comprende la estructura real de SYSTEM_PROCESS_INFORMATION

**Compilación y prueba (2026-11-23)**
- Compilado con `x86_64-w64-mingw32-gcc` desde el Home-Server (recién instalado mingw-w64) y desde Kali. DLL: `-shared -Wall`; test: `-Wall -municode`
- Prueba en VM: `$rkping.exe` (PID 5704) visible antes de cargar la DLL, **invisible después**. `test_hook.exe` carga la DLL con LoadLibrary, el DllMain instala el inline hook de NtQuerySystemInformation, y la segunda enumeración de procesos ya no muestra el proceso oculto
- **Fase 2 completada**: el inline hook de NtQuerySystemInformation filtra correctamente procesos por prefijo configurable

### Fase 3 — inyector de DLL

**Fecha**: 2026-11-23

**Paso 1 — Diseño (Qwen 3.5, modelo `llm-cpp`)**
- Prompt: diseñar cabecera para inyector DLL con enable_debug_privilege, inject_dll e inject_all
- Resultado: razonable pero sobre-ingeniería. Creó un InjectorContext struct innecesario (la skip list es estática), caching del token handle sin motivo, `extern "C"` (C++), y los nombres en la skip list sin extensión `.exe` (szExeFile devuelve `csrss.exe`, no `csrss`)
- Tiempo: ~90 s

**Revisión de Claude**: se simplificó la cabecera a 3 funciones sin estado: `enable_debug_privilege`, `inject_dll`, `inject_all`

**Paso 2 — Implementación (Qwen 2.5, modelo `llm-cpp-fast`)**
- Resultado: significativamente mejor que en la Fase 2. El código es funcionalmente correcto con solo 3 problemas menores:
  1. Falta `#include "injector.h"`
  2. Array `skip_list` definido pero no usado — las comparaciones están hardcodeadas en `inject_all`
  3. No salta PID 0 (System Idle Process)
- Tiempo: ~45 s

**Correcciones de Claude**: mínimas — añadido include, refactorizada la skip list con función `is_skipped()` que itera el array, añadido check de PID 0, añadido wprintf de feedback por proceso inyectado. `inject_main.c` escrito directamente por Claude (punto de entrada con dos modos: PID único o todos)

**Valoración**: la calidad del 2.5 mejora notablemente cuando la tarea usa APIs Win32 estándar bien documentadas (OpenProcess, VirtualAllocEx, CreateRemoteThread). El patrón de inyección DLL clásico es probablemente parte de su entrenamiento. Contraste claro con la Fase 2 donde el inline hooking x64 (tarea de bajo nivel poco documentada) produjo código inservible

### Fase 4 — auto-ocultación
(pendiente)

### Fase 5 — integración
(pendiente)
