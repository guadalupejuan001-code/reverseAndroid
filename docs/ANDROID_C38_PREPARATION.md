# C38 · ejecución real hacia Zygote con la CPU XI acelerada

## Estado previo
C36 reanuda desde C35 limpio con KeyMint en `libcrypto.so` y mantiene comprobación de propiedades odsign y procesos. C37 implementó caché ARM64 física coherente, pero **solo podrá usarse si la prueba diferencial de ejecución real es correcta**.

## Condiciones de arranque de C38
El workflow `.github/workflows/android-c38-zygote.yml` no se ejecuta por el simple hecho de crear esta rama. Está condicionado a la posterior creación de `docs/ANDROID_C38_EXECUTION_LAUNCH.md`, la cual solo debe producirse tras verificar:

1. C36 run [41352354242](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41352354242) terminado con éxito y artefactos `reverse-c36-keymint-continuation` y `reverse-c36-keymint-diagnostics` completos.
2. C37 run [41353600067](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41353600067) terminado con éxito y demostración de identidad byte por byte entre el intérprete original y el de caché, ejecutando cinco millones de instrucciones ARM64 desde el mismo checkpoint C35.
3. Una nueva autenticación independiente del contrato de cuatro archivos de C36 en el mismo run, SHA fuente, progreso CPU, procedencia, ausencia de reboot/fallos y `actual-stage.json`.
4. Compilación del intérprete ARM64 XI original con caché ya validada, sin modificar el GKI, RAM, reloj virtual de 1 MHz, SELinux ni medios de Android.
5. Ejecución de CPU **genuina** desde C36, tres presupuestos acotados; evaluación de `odsign.key.done`, `odsign.verification.done`, `zygote64`, `system_server`, `surfaceflinger`, `sys.boot_completed` y framebuffer en el nuevo checkpoint.

No hacer `setprop` artificial, no forzar Zygote, no reutilizar un checkpoint de error ni declarar un Android completo antes de sus señales reales. No se edita `main`.
