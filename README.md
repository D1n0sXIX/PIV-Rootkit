# PIV-Rootkit

Rootkit de modo usuario (ring 3) para Windows, desarrollado como proyecto del bloque 1 de **Proyectos de Ciberseguridad (PCIB)** en U-tad (curso 2026-27). Inspirado en [r77-rootkit](https://github.com/bytecode77/r77-rootkit) de bytecode77, con modificaciones propias: los prefijos de ocultación se leen de un fichero `.txt` en vez de estar hardcodeados.

## Qué hace

Oculta procesos, ficheros y claves de registro cuyo nombre empiece por alguno de los prefijos definidos en `config.txt`. Funciona interceptando las funciones nativas de enumeración de Windows (`NtQuerySystemInformation`, `NtQueryDirectoryFile`, etc.) mediante inyección de DLL en los procesos que consultan el sistema.

**Solo para entorno de laboratorio aislado** (VMs sin red, con snapshots). No es una herramienta de producción.

## Objetivo académico

Este proyecto tiene dos objetivos:

1. **Banco de visibilidad**: medir qué rastros deja la ocultación en modo usuario frente a la telemetría defensiva (Sysmon, ETW, Defender, System Informer). Los resultados alimentan la matriz de visibilidad del hito H6.
2. **Estudio de capacidades de IA local**: el desarrollo se hace con modelos locales abliterated (sin censura) corriendo en un Home-Server con una RX 580 8 GB, para medir hasta dónde llegan sin intervención de modelos comerciales en la parte ofensiva.

## Metodología de desarrollo

El código lo generan dos modelos locales, con revisión externa:

| Rol | Modelo | Tarea |
|---|---|---|
| **Arquitecto** | `huihui_ai/qwen3.5-abliterated:9b` (Ollama, Vulkan, think=false) | Diseña la interfaz de cada módulo: structs, firmas de funciones, flujo |
| **Codificador** | `huihui_ai/qwen2.5-coder-abliterate:7b` (Ollama, Vulkan, think=false) | Escribe el código C de cada módulo a partir de la spec del arquitecto |
| **Revisor** | Claude (Opus 4.6, remoto) | Revisa cada entrega: bugs, APIs mal usadas, lógica rota, alucinaciones; corrige lo necesario |
| **Orquestador** | D1n0 (humano) | Pasa prompts entre modelos, compila en Visual Studio, prueba en la VM del lab |

Cada fase se documenta con: qué prompt se dio a cada modelo, qué devolvió, qué corrigió el revisor, y el resultado de compilación y pruebas. Esta bitácora vive en la memoria del proyecto en D1n0s-University (hito H4 de PCIB).

## Arquitectura modular

El desarrollo se divide en 5 fases, cada una con sus propios módulos, commit y prueba en el laboratorio:

### Fase 1 — Detección de procesos por prefijo
- `src/config_reader.c/.h`: lee prefijos de `config.txt` (uno por línea)
- `src/process_scanner.c/.h`: enumera procesos con la API de Windows y compara contra los prefijos
- `src/main.c`: ejecutable de consola que lista los procesos que se ocultarían
- **Prueba**: ejecutar en la VM, lanzar procesos con nombres que empiecen por un prefijo, comprobar que los detecta

### Fase 2 — Hook DLL
- `src/hook_engine.c/.h`: inline hooking de `NtQuerySystemInformation`
- `src/filter_logic.c/.h`: recorre la lista enlazada `SYSTEM_PROCESS_INFORMATION` y desenlaza las entradas que coincidan con un prefijo
- **Prueba**: cargar la DLL manualmente (con System Informer) en un proceso y verificar que deja de ver los procesos con prefijo

### Fase 3 — Inyector
- `src/injector.c/.h`: inyección clásica de DLL (`OpenProcess` + `VirtualAllocEx` + `WriteProcessMemory` + `CreateRemoteThread`)
- **Prueba**: inyectar la hook DLL en `taskmgr.exe`, `cmd.exe` y PowerShell; verificar ocultación

### Fase 4 — Auto-ocultación
- `src/self_hide.c/.h`: el rootkit se añade a la lista de prefijos para ocultarse a sí mismo
- **Prueba**: el rootkit corre y no aparece en las enumeraciones de procesos

### Fase 5 — Integración y medición
- Unión de todos los módulos
- Medición en la VM con Sysmon activo:
  - `Get-Process`: ¿ve los procesos ocultos? (no debería)
  - Acceso directo por PID: ¿los ve? (debería)
  - Sysmon evento 1: ¿registró su creación? (debería)
  - System Informer: ¿los ve? (depende de si se inyectó en él)
- Resultados → matriz de visibilidad (H6)

## Entorno de laboratorio

- **VM**: Windows Server 2019, VirtualBox, BIOS clásica, sin red
- **Defender**: desactivado (`RealTimeProtectionEnabled`, `BehaviorMonitorEnabled`, `IsTamperProtected` = False)
- **Sysmon**: v15.22, config SwiftOnSecurity
- **System Informer**: herramienta de inspección directa (sustituyendo a OpenArk)
- **Snapshots**: S0 (base limpia) → S1 (pre-acción) → S2 (post-acción) → restaurar

## Compilación

Compilar con Visual Studio (MSVC) en la VM del laboratorio. Cada fase tiene su propio target:

```
# Fase 1: ejecutable de consola
cl /W4 /Fe:scanner.exe src/main.c src/config_reader.c src/process_scanner.c

# Fase 2: DLL de hooks
cl /W4 /LD /Fe:hook.dll src/hook_engine.c src/filter_logic.c src/config_reader.c

# Fase 3+: ejecutable inyector
cl /W4 /Fe:injector.exe src/injector.c
```

## Estructura del repositorio

```
PIV-Rootkit/
├── README.md
├── config.txt          # prefijos de ocultación (uno por línea)
├── src/
│   ├── config_reader.c
│   ├── config_reader.h
│   ├── process_scanner.c
│   ├── process_scanner.h
│   ├── hook_engine.c
│   ├── hook_engine.h
│   ├── filter_logic.c
│   ├── filter_logic.h
│   ├── injector.c
│   ├── injector.h
│   ├── self_hide.c
│   ├── self_hide.h
│   └── main.c
├── docs/
│   └── USO-IA.md       # registro de uso de IA por fase
└── test/
    └── test_config.txt  # prefijos de prueba
```

## Licencia

Proyecto académico. No redistribuir fuera del contexto de la asignatura.
