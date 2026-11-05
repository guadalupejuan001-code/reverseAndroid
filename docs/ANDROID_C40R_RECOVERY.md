# C40R — recuperación auténtica desde C39 sin pérdida del checkpoint anterior

## Causa verificada del fallo C40

Run original: https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41363261111

- La ejecución produjo trazas hasta 19.066.342.718 instrucciones frente a 18.393.448.839 de C39, por lo que **sí ejecutó CPU real**.
- El paso `Resume native Android...` salió con código 2. El resumen descargado informó `clean_completed=false`, `continuation_error=null` y un SHA de estado de diagnóstico; **no hubo contrato de salida**.
- El ZIP diagnóstico **no contenía** `native/process-outcome.json`, `native/current.json`, `native/result.json` ni `native/progress.log`. Por ello el motivo exacto (detención por supervisión, transición inválida, terminación huésped, presupuesto o error de proceso) no se podía distinguir sin repetir la ejecución.
- C41 rechazó correctamente el resultado de C40. No debe cambiarse ese rechazo por un salto de seguridad, ni promover el SHA intermedio de C40.
- Última fuente plenamente verificada: C39, SHA-256 `094d5c9b9532fbb21ea029031b952f46c4df0e589c2f0d75a4dd64677bc49438`, 18.393.448.839 instrucciones, run 41360328706.

## Corrección de recuperación y diagnóstico

El nuevo archivo `tools/android_c40_recovery_evidence.mjs` realiza exclusivamente lectura y verificación:

- Autentica el contrato C39 de cuatro archivos antes de emitir un diagnóstico.
- Lee `journal/latest.json`, valida número de generación, SHA-256 del checkpoint mediante lectura por bloques de 1 MiB, y comprueba identidad entre puntero, estado, informe y procedencia.
- Lee el resultado del supervisor (exit_code, trigger, señal, forzado de terminación), la parada del invitado, su PC y los marcadores de progreso.
- Clasifica host watchdog, fallo de proceso, parada arquitectónica, pánico de Android o diario sin integridad. Los informes **no son autorización automática de continuación**.
- Integra `recovery-evidence.json` y los diagnósticos con `android_c27_sequence.mjs`. Conserva el contrato de salida original: no hay `output-contract.json` sin proceso completado, progreso aceptado y preflight limpio.

## Reanudación C39 — C40R1, R2 y R3

El workflow `android-c40r-recovery.yml` restaura la imagen/disco original y compila la misma CPU XI/Ether, sin cambios en kernel, AVB, SELinux, KeyMint, reloj, memoria ni JNI.

Ejecuta tres continuaciones **separadas** de hasta 250.000.000 instrucciones desde la última fuente limpia: primero C39; luego solo desde R1 si R1 terminó correctamente; luego desde R2 si R2 fue aprobado.

Al finalizar **cada** etapa se comprueba el contrato completo, hashes, reporte del supervisor, señales de seguridad, proceso `app_process64 --zygote`, presencia de `zygote64` y `system_server`, y se sube el checkpoint limpio de esa etapa. Si R2 o R3 falla, **el último checkpoint exitoso de R1/R2 queda publicado** sin ser sustituido por un candidato parcial.

Siempre se publican por separado los logs nativos, resultado de proceso y evidencia del diario de cada etapa para conocer el siguiente problema real. Los snapshots de estados fallidos no se anuncian como recuperables hasta pasar sus comprobaciones.

No se interpreta un workflow aprobado como evidencia de Zygote listo ni de Android arrancado. La rama permanece experimental; `main` no se modifica.
