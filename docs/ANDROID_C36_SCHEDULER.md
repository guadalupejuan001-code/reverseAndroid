# C36 — verificación del planificador antes de corregir Rust Binder / KeyMint

## Evidencia
- C34: 14,807,298,839 instrucciones, 238,867,464 ticks virtuales, 328,937 excepciones.
- C35: 15,317,348,839 instrucciones, 246,990,946 ticks virtuales, 331,016 excepciones.
- Incremento: 510,050,000 instrucciones, 8,123,482 ticks virtuales (8.123482 segundos a **1 MHz**) y 2,079 excepciones.
- El hilo actual permanece en PID/TID 409 `android.hardwar` en ambos checkpoints; el proceso `odsign` PID 554 no registró cambios de contexto ni CPU ejecutada.
- `odsign`, `keystore2` y otros hilos de hardware están durmiendo en `ioctl(BINDER_WRITE_READ)` sobre `/dev/binderfs/binder`.
- `odsign.key.done` y `odsign.verification.done` no están presentes, `apexd.status=activated`.
- `binder.impl=rust` se observa en bootargs del GKI. Por tanto, **no** son válidas las lecturas de `struct binder_proc` clásica dentro de `file.private_data` de esa instancia. Sus resultados no demuestran corrupción.

## Prueba real
La CI valida el contrato original C35, decodifica en lectura exclusiva las estructuras BTF del kernel real y observa el hilo 409, su grupo y ejecutable, política/prioridad de planificación, el estado de `odsign` y `keystore2`, y el temporizador/GIC. Si un campo no está disponible se indica sin inventarlo.

## Criterio de corrección
No forzar `odsign.key.done`, no desactivar SELinux ni inventar Zygote. Si el hilo 409 tiene prioridad RT y monopoliza la CPU, instrumentar y probar la interacción de planificación real. Si es trabajo legítimo de KeyMint o un lazo con alta ocupación, localizar y optimizar el punto exacto en XI/Ether o su entorno. Un test de lectura de un checkpoint **no** implica avance del boot.
