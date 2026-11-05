# Android C32 — corrección de observación UART por continuación nativa

## Evidencia original, sin reejecutar la CPU

La ejecución [C32 de GitHub Actions](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41338225255) concluyó correctamente desde C31:
**13,867,398,839 instrucciones**, +169,400,000, nuevo checkpoint
`721a228423e55810f11dc4295c64e41feca1fb2121d6db8a0666a6e219ad1a53`.
El checkpoint se conserva inmutable y se enlaza con el estado fuente C31,
`3011b7caaf537657d980b11ae6a1f3f84cc7a1cce180d9e808fe9c8a3d8864cc`.

La primera política C32 comparó incorrectamente `report.console` con el
prefijo de `sourceReport.console` y por ello informó
`console_delta_not_proven`. El programa nativo
`tools/guest_budget_cycles.hpp` crea `previous_console` vacío **para cada
llamada de continuación** y sólo concatena eventos UART dentro de los
presupuestos de esa misma llamada. Por tanto, `report.console` C32
**es el registro nuevo del ciclo C32**, no un prefijo acumulado desde C31.
La cadena de procedencia `input_checkpoint_sha256`,
`input_report_sha256`, restablecimiento byte-idéntico y contratos de cuatro
archivos autentica la reanudación.

El registro exacto contenido en C32 dice:

- Tiempo invitado **218.981795**: `derive_classpath` (PID 552) terminó con código 0.
- Tiempo invitado **220.058738**: `art_boot` (PID 553) terminó con código 0.
- Tiempo invitado **220.162918**: `odsign` comenzó como PID 554.
- El observador ligado al checkpoint encuentra al proceso `odsign` PID 554
  vivo, en ejecución de usuario, con estado 0 y sin exit_state.
- SELinux sigue imponiendo la política (enforcing).
- Aún faltan `zygote64`, `system_server`, `surfaceflinger`,
  `sys.boot_completed=1` y un scanout; no se declara Launcher, pantalla o APK.

## Corrección y pruebas

`tools/android_c32_stage.mjs` versión 2 valida ahora de forma explícita
**ambos** contratos completos, la relación de ejecución C31→C32, la ausencia
de alteraciones de CPU/guest, y clasifica el UART por presupuesto nativo.
Distingue salida limpia real de `derive_classpath` y `art_boot` del retorno
de `exec_start`, y exige un proceso `odsign` vivo para clasificar
`odsign_running_after_art_boot`.

Las pruebas de host descartan fuentes diferentes, comparaciones cruzadas,
procesos zombis y una falsa señal de salida gráfica. El workflow
`android-c32-uart-audit.yml` descarga exclusivamente **los mismos**
checkpoints C31 y C32 para repetir la comprobación sin nuevas instrucciones,
sin tocar el disco y sin sustituir el emulador XI/Ether.

Una ejecución CI positiva avala la lectura del UART y del checkpoint, no
confirma un arranque gráfico completo de Android.
