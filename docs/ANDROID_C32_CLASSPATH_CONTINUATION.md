# Ciclo C32 — continuación auténtica después de derive_classpath

## Fuente comprobada

C31 se ejecutó en GitHub Actions [41334664768](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41334664768) y produjo la frontera limpia de **13,697,998,839 instrucciones** con checkpoint SHA-256
`3011b7caaf537657d980b11ae6a1f3f84cc7a1cce180d9e808fe9c8a3d8864cc`.
El commit productor fue `87c79bd86624268eac1f2b37a71d874355703e0b`.
El artefacto `reverse-c31-authenticated-continuation` (ID `13095206564`)
incluye el estado real de la CPU, RAM, dispositivos, deltas de bloque, bitácora
y contrato para reanudación.

C31 probó que `restorecon --recursive --skip-ce /data` terminó con éxito,
y que `derive_classpath` comenzó con PID 552 durante `post-fs-data`.
**El retorno exitoso de `exec_start derive_classpath` no prueba que el
proceso `derive_classpath` terminó.** La verificación C32 distingue estos eventos.

## Alcance implementado

- Verificar de nuevo los cuatro archivos firmemente vinculados por hash, la
  máquina C27 original, el arranque `/init`, disco, RAM, registro PC y reloj.
- Usar el build probado C29 como herramienta de compilación original (no como
  checkpoint), además del disco original C03 y el perfil C27 con userdata vacía.
- No cambiar `src`, XI, Ether, CPU AArch64, PCI, GPU, disco, kernel, SELinux,
  AVB, bootconfig ni cadencia del contador.
- Reanudar desde C31 por **una** ejecución acotada por 180,000 ms de
  presupuesto nativo y 300,000,000 instrucciones; salida limpia obligatoria.
- Calcular progreso solamente con el estado auténtico C32, el observador
  read-only del kernel y el frame ligado al mismo checkpoint.
- Anotar el fin de `derive_classpath` únicamente si el **nuevo tramo**
  de consola contiene `Service 'derive_classpath' ... exited with status 0`.
- Verificar existencia de `zygote64`, `system_server`, `surfaceflinger`
  como procesos de usuarios vivos; exigir `sys.boot_completed=1` y scanout
  real para pasar a inspección visual, sin declarar de forma automática que
  los píxeles contienen un Launcher.

## Resultado exigido

La nueva frontera solo será aceptable si el proceso nativo termina
correctamente, hay progreso CPU real, todos los SHA/procedencias se mantienen
consistentes y el guest no reinicia ni presenta un fallo fatal. C32 publicará
`reverse-c32-authenticated-continuation` con un contrato para una eventual
C33 y `reverse-c32-diagnostics` con la lectura de etapa. Si CI falla,
conservar y diagnosticar la evidencia sin tratarla como una continuación sana.

El objetivo posterior sigue siendo framework Android y salida gráfica
sin falsificar rendimiento o implementar un escritorio que no venga del
Android invitado. No se afirma jugabilidad de APK ni Wild Rift todavía.
