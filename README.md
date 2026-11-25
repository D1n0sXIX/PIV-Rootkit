# PIV-Rootkit

Rootkit de modo usuario (ring 3) para Windows x64, desarrollado como proyecto del bloque 1 de **Proyectos de Ciberseguridad (PCIB)** en U-tad (curso 2026-27). Inspirado en [r77-rootkit](https://github.com/bytecode77/r77-rootkit), con modificaciones propias: los prefijos de ocultación se leen de un fichero de texto en vez de estar hardcodeados.

**Solo para entorno de laboratorio aislado** (VMs sin red, con snapshots). No es una herramienta de producción.

## Qué hace

Oculta procesos y ficheros cuyo nombre empiece por alguno de los prefijos definidos en `config.txt`. Funciona interceptando funciones nativas de Windows mediante inline hooking e inyección de DLL:

- **Procesos**: hook de `NtQuerySystemInformation` — las enumeraciones (`Get-Process`, `tasklist`, Task Manager) no muestran los procesos ocultos
- **Ficheros**: hook de `NtQueryDirectoryFile` — `dir`, Explorer y `Get-ChildItem` no listan los ficheros ocultos (pero `Test-Path` y el acceso por nombre exacto siguen funcionando)
- **Persistencia**: clave `HKCU\...\Run` que reinyecta el rootkit al iniciar sesión
- **Instalación de un tiro**: `install.exe` copia ficheros con prefijo, instala persistencia e inyecta en todos los procesos; `uninstall.exe` lo revierte

## Objetivo académico

1. **Banco de visibilidad**: medir qué rastros deja la ocultación en modo usuario frente a la telemetría defensiva (Sysmon, ETW, System Informer). Los resultados alimentan la matriz de visibilidad del hito H6.
2. **Estudio de capacidades de IA local**: el desarrollo se hace con modelos locales abliterated como generadores de código, con Claude como revisor técnico, para medir hasta dónde llegan sin intervención de modelos comerciales en la parte ofensiva.

## Metodología de desarrollo

| Rol | Modelo | Tarea |
|---|---|---|
| **Arquitecto** | `huihui_ai/qwen3.5-abliterated:9b` (Ollama, Vulkan, think=false) | Diseña interfaces de cada módulo: structs, firmas, flujo |
| **Codificador** | `huihui_ai/qwen2.5-coder-abliterate:7b` (Ollama, Vulkan, think=false) | Escribe el código C a partir de la spec del arquitecto |
| **Revisor** | Claude Opus 4.6 (remoto, Anthropic) | Revisa cada entrega: bugs, APIs mal usadas, lógica rota; corrige lo necesario |
| **Orquestador** | D1n0 (humano) | Pasa prompts entre modelos, compila, prueba en la VM del lab y captura evidencias |

Bitácora detallada por fase en [`docs/USO-IA.md`](docs/USO-IA.md).

## Fases de desarrollo

| Fase | Módulos | Qué hace | Estado |
|---|---|---|---|
| 1 | `config_reader` + `process_scanner` | Lee prefijos de `config.txt`, enumera procesos del sistema y matchea por prefijo | Completada |
| 2 | `hook` + `ntdefs` + `dllmain` | Inline hook x64 de `NtQuerySystemInformation`: filtra procesos de la lista enlazada por prefijo | Completada |
| 3 | `injector` | Inyección DLL clásica (`OpenProcess` + `VirtualAllocEx` + `WriteProcessMemory` + `CreateRemoteThread`) en un PID o en todos | Completada |
| 4 | (sin código nuevo) | Auto-ocultación: los binarios del rootkit se renombran con el prefijo (`$rkinjector.exe`) y el propio hook los filtra | Completada |
| 5 | Integración | Medición completa con Sysmon: config estándar (Ronda A) vs config parcheada (Ronda B). Matriz de visibilidad | Completada |
| 6 | `file_hide` | Inline hook de `NtQueryDirectoryFile`: oculta ficheros y carpetas con prefijo de la enumeración de directorio | Compilada, pendiente de probar |
| 7 | `persist` | Persistencia vía clave `HKCU\...\Run`: instala, comprueba y elimina una entrada de autoarranque | Compilada, pendiente de probar |
| 8 | `install` + `uninstall` | `install.exe`: copia ficheros con prefijo + instala Run + inyecta en todos. `uninstall.exe`: elimina Run + borra ficheros con prefijo | Compilada, pendiente de probar |

## Entorno de laboratorio

- **VM**: Windows Server 2019, VirtualBox, BIOS clásica, sin red
- **Defender**: desactivado (`RealTimeProtectionEnabled`, `BehaviorMonitorEnabled`, `IsTamperProtected` = False)
- **Sysmon**: v15.22, dos configs:
  - `sysmonconfig-export.xml`: SwiftOnSecurity v74 original (ciega a ev7, ev8 por exclusión de kernel32.dll, ev10)
  - `sysmonconfig-lab.xml`: parcheada con ev7 para `C:\lab\`, sin exclusión de kernel32.dll en ev8, ev10 para `C:\lab\`
- **System Informer**: inspección directa de procesos (driver en kernel, no afectado por hooks de modo usuario)
- **Snapshots**: S0 (base limpia) → S1 (pre-acción) → S2 (post-acción) → restaurar

## Compilación

Cross-compilar desde Linux (Kali o Home-Server) con `x86_64-w64-mingw32-gcc`. Pasar los binarios a la VM vía carpeta compartida de VirtualBox.

```bash
cd src/

# hook.dll (DLL con los hooks de NtQuerySystemInformation y NtQueryDirectoryFile)
x86_64-w64-mingw32-gcc -shared -Wall -o hook.dll dllmain.c hook.c file_hide.c config_reader.c -lntdll

# injector.exe (inyecta hook.dll en un PID o en todos)
x86_64-w64-mingw32-gcc -Wall -municode -o injector.exe inject_main.c injector.c -ladvapi32

# scanner.exe (enumera procesos y detecta los que matchean por prefijo)
x86_64-w64-mingw32-gcc -Wall -municode -o scanner.exe main.c config_reader.c process_scanner.c

# persist.exe (CLI de persistencia: install/remove/check de clave Run)
x86_64-w64-mingw32-gcc -Wall -municode -o persist.exe persist_main.c persist.c -ladvapi32

# install.exe (copia con prefijo + Run + inyecta, todo de un tiro)
x86_64-w64-mingw32-gcc -Wall -municode -o install.exe install.c config_reader.c injector.c persist.c -ladvapi32

# uninstall.exe (elimina Run + borra ficheros con prefijo)
x86_64-w64-mingw32-gcc -Wall -municode -o uninstall.exe uninstall.c config_reader.c persist.c -ladvapi32

# Tests (opcionales)
x86_64-w64-mingw32-gcc -Wall -municode -o test_hook.exe test_hook.c config_reader.c -lntdll
x86_64-w64-mingw32-gcc -Wall -municode -o test_inject.exe test_inject.c config_reader.c process_scanner.c
```

## Uso en la VM del laboratorio

```powershell
# Copiar binarios y config a C:\lab\
# (desde la carpeta compartida de VirtualBox)

# 1. Probar ocultación de procesos
C:\lab\injector.exe C:\lab\hook.dll <PID>    # inyectar en un proceso
C:\lab\injector.exe C:\lab\hook.dll           # inyectar en todos

# 2. Probar ocultación de ficheros (tras inyectar)
dir C:\lab\                                    # no muestra ficheros con prefijo
Test-Path 'C:\lab\$rk_test.txt'               # True (acceso directo sigue funcionando)

# 3. Persistencia
C:\lab\persist.exe install WindowsUpdate "C:\lab\injector.exe C:\lab\hook.dll"
C:\lab\persist.exe check WindowsUpdate
C:\lab\persist.exe remove WindowsUpdate

# 4. Instalación completa
C:\lab\install.exe                             # copia + Run + inyecta
C:\lab\uninstall.exe                           # limpia todo

# 5. Dashboard (genera HTML con datos de procesos, ficheros, registro y Sysmon)
powershell -ExecutionPolicy Bypass -File C:\lab\dashboard.ps1
```

## Estructura del repositorio

```
PIV-Rootkit/
├── README.md
├── config.txt                  # prefijos de ocultación (uno por línea: $rk, $hide, $test)
├── .gitignore
├── src/
│   ├── config_reader.c/.h      # carga prefijos de config.txt
│   ├── process_scanner.c/.h    # enumera procesos y matchea por prefijo
│   ├── ntdefs.h                # structs NT no exportados (SYSTEM_PROCESS_INFORMATION, etc.)
│   ├── hook.c/.h               # inline hook x64 de NtQuerySystemInformation
│   ├── file_hide.c/.h          # inline hook x64 de NtQueryDirectoryFile
│   ├── dllmain.c               # punto de entrada de hook.dll (instala ambos hooks)
│   ├── injector.c/.h           # inyección DLL clásica (OpenProcess + CreateRemoteThread)
│   ├── inject_main.c           # CLI del inyector
│   ├── persist.c/.h            # persistencia vía HKCU\...\Run
│   ├── persist_main.c          # CLI de persistencia
│   ├── install.c               # instalador todo-en-uno
│   ├── uninstall.c             # desinstalador
│   ├── main.c                  # CLI del scanner de procesos
│   ├── test_hook.c             # test unitario del hook (carga DLL y enumera)
│   └── test_inject.c           # test unitario de inyección
├── dashboard/
│   ├── dashboard.ps1           # genera HTML con datos reales (procesos, ficheros, registro, Sysmon)
│   └── index.html              # plantilla del dashboard
├── docs/
│   └── USO-IA.md               # registro de uso de IA por fase (punto 3 de la memoria)
├── test/
│   └── test_config.txt         # prefijos de prueba
├── sysmonconfig-export.xml     # config Sysmon original (SwiftOnSecurity v74)
└── sysmonconfig-lab.xml        # config Sysmon parcheada para el laboratorio
```

## Limitaciones conocidas

- Solo modo usuario (ring 3): herramientas con driver de kernel (Sysmon, System Informer) ven todo
- Inyectar en la propia PowerShell causa crash (race condition)
- La tabla de prefijos se carga una vez al inyectar; cambiar `config.txt` requiere reinyectar
- Los procesos hijos no heredan el hook
- La inyección masiva puede saturar la VM (~30 procesos hooked + cascada de WerFault)
- `NtOpenProcess` no está hookeado: un analista puede abrir el proceso oculto por PID si lo conoce

## Licencia

Proyecto académico. No redistribuir fuera del contexto de la asignatura.
