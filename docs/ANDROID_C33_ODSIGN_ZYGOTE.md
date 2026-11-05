# C33 — continuación verificable de odsign hacia Zygote

## Estado auténtico de entrada

- Origen: ejecución Android C32 de [GitHub Actions 41338225255](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41338225255).
- Artefacto de entrada exacto: `reverse-c32-authenticated-continuation`, ID `13096915647`.
- Checkpoint SHA-256: `721a228423e55810f11dc4295c64e41feca1fb2121d6db8a0666a6e219ad1a53`.
- Contador CPU: **13,867,398,839** instrucciones ejecutadas.
- Commit productor: `48f1e18f30e9072d81cbb73ab4724f317d5613c6`.
- El segundo auditor [C32 UART](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41339632949) confirmó los cuatro archivos originales, el registro UART por ejecución, `derive_classpath` y `art_boot` finalizados con estado 0 y `odsign` (PID 554) vivo.
- No se había verificado `zygote64`, `system_server`, `surfaceflinger`, `sys.boot_completed=1` ni scanout real.

## Operación C33

La máquina no se reinicia. El workflow debe descargar el checkpoint C32, validar su contrato de **cuatro hashes** (checkpoint, reporte, resultado y host), conservar el origen C27 autenticado, la imagen PCI original, el disco completo, la RAM lógica de 2 GiB, CPU ARM64, AVB, SELinux y reloj con semilla y fase originales. Reconstruye el mismo medio de datos C27 desde la imagen original y compila el mismo ejecutable XI/Ether, sin cambiar `src`, `third_party`, `scripts`, `patches` ni las fuentes nativas invitadas.

El presupuesto es de **dos rondas consecutivas** de hasta 180 s cada una y como máximo 350 millones de instrucciones cada ronda, con checkpoints intermedios. Es una ejecución real, no una prueba sintética de `init`. Las políticas de supervisión rechazan errores de CPU, ausencia de progreso, host matado, falta de aceptación de salida o reboot Android.

Al terminar, los observadores read-only analizan el **mismo** checkpoint, verifican la propiedad real de PID 1 y el scanout exacto. `tools/android_c33_stage.mjs` solo informa:

- `odsign_exited_clean` si el UART nuevo muestra explícitamente `Service 'odsign' ... exited with status 0`.
- `odsign_exited_error` cuando la salida de ese servicio muestra estado distinto de 0.
- `zygote_start_requested` si `init` inicia el servicio Zygote, sin confundir una solicitud con un proceso vivo.
- `framework_processes_running` cuando hay procesos reales de usuarios comprobados por su estado vivo.
- `framework_verified` exclusivamente cuando la política independiente de framework confirma todos sus requisitos.
- Un framebuffer exportado y vinculado por SHA nunca se declara Launcher sin revisión visual.

La prueba de CI puede certificar una nueva frontera CPU sana aun cuando Android siga preparando clases o servicios. El contrato de salida publicado debe ser utilizado para una continuación ulterior; ningún contador o SHA de salida puede predecirse ni inventarse.

## Evidencia que se publicará

- Workflow: `android-c33-odsign-zygote.yml`.
- `reverse-c33-authenticated-continuation`: nuevo checkpoint completo, contrato y observaciones.
- `reverse-c33-stage-diagnostics`: auditoría de origen, pruebas de host, cuadro de procesos y resumen de etapa.

La rama experimental no cambia `main`; no se afirma APK de emulador instalable ni Wild Rift antes de que Android arranque.
