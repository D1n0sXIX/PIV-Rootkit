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

### Fase 2 — hook DLL
(pendiente)

### Fase 3 — inyector
(pendiente)

### Fase 4 — auto-ocultación
(pendiente)

### Fase 5 — integración
(pendiente)
