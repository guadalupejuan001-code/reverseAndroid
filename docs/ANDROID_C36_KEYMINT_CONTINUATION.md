# Android C36 · KeyMint crypto -> odsign -> Zygote

## Diagnóstico autenticado
- C35 auténtico: 15,317,348,839 instrucciones; checkpoint `c4bd2ff6b33a3ec79ff7ced57e6e248c848e4b31374cc4a04dc46de8f36b6c8e`.
- `odsign` espera el resultado de KeyStore/KeyMint mediante `BINDER_WRITE_READ`.
- TID 409 pertenece al proceso `/apex/com.android.hardware.keymint/bin/hw/android.hardware.security.keymint-service.nonsecure` (TGID 403).
- CPU en EL0 ejecutando la biblioteca `/apex/com.android.hardware.keymint/lib64/libcrypto.so`, aproximadamente offset de archivo 1,372,500 al finalizar C35.
- Scheduling real: policy 0, fair_sched_class, prioridad 120; no prioridad RT. El timer virtual está habilitado a 1 MHz. Ninguna evidencia demuestra un defecto de Rust Binder o del planificador.
- `init.svc.odsign=running`; `odsign.key.done` y `odsign.verification.done` todavía no existen.

## Continuación sin falsos éxitos
El ciclo reanuda el mismo checkpoint real C35. Usa tres rondas acotadas de 540 s y 600 M instrucciones máximas cada una en el mismo intérprete ARM64 XI/Ether. No cambia CPU, reloj, GKI, disco, RAM, servicios, clave ni SELinux. El intérprete podrá avanzar la operación criptográfica de KeyMint sin manipular sus resultados.

La CI valida source + output (contratos de cuatro archivos), ejecución nativa y progreso, procesos `odsign` y `zygote64`, reporta `sys.boot_completed` y framebuffer solo a partir de observaciones del checkpoint.

**Condición:** el resultado de una CI correcta solo prueba avance de CPU/checkpoint. No prueba `odsign` terminado ni Zygote si no hay procesos reales. La imagen terminal de una excepción arquitectónica no es reutilizable.

## Referencias
- CI C35: https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41347214737
- CI de scheduler y ruta ejecutable C36: https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41352090038
- No lanzar Android desde cero ni fusionar `main` sin tests de compatibilidad.
