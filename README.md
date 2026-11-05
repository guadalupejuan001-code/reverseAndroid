# reverseAndroid

Repositorio propio del **emulador Android de REVERSE**, con el modelo compacto XI/Ether original.

La base C68 completa procede de [REVERSE-PC, commit 37f5b461](https://github.com/guadalupejuan001-code/REVERSE-PC/tree/37f5b4610c5c31981e359fcf8fd0b303c14f01fd). Sus 870 archivos se conservan y se comprueban por hash en `state/upstream-snapshot.json`. Los flujos históricos están en `docs/upstream-workflows`; aquí solo se ejecutan los del emulador. La documentación anterior se conserva en `docs/REVERSE_PC_README.md`.

## Último resultado verificado

[C73, etapa 6](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42077412765) verificó **`system_server` vivo, PID 3340**, en **76.895.950.532 instrucciones**. El checkpoint `a414430675f8d2e706c6e60a22599b60eab4a16956af9574dc7210b7814c798d` coincide con la ejecución local y está conservado íntegramente en cuatro piezas, con la admisión y el decodificador originales. SELinux sigue activo. El arranque completo y la interfaz continúan pendientes de verificar; C74 avanza desde esa frontera sin entradas UART nuevas.

C74 alcanzó localmente **80.895.950.532 instrucciones** con la admisión original completa. La [lectura de sus hilos](docs/evidence/c74/stage4-threads-summary.json) confirma que SystemServer pasó de 12 a 38 hilos: aparecen ActivityManager, android.ui y android.display. La [última captura inspeccionada, de la etapa 2](docs/evidence/c74/stage2-original-scanout.png), muestra la animación de inicio. Las tres primeras etapas independientes reprodujeron los mismos SHA-256 de los snapshots completos y conservaron las cuatro piezas de cada estado; sus [registros](docs/evidence/c74/independent-stage3-replay.json) quedan fijados en el repositorio. El arranque completo sigue pendiente.

[C72, etapas 1–6](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42062131978) completó su última frontera original en **70.895.950.532 instrucciones**, con checkpoint `18a5147749f4d13c776fee9bbb313fc20422357c46a9da6deaf2d8b450e1084b`. Los hashes de los seis snapshots completos coinciden con la ejecución local. En la etapa 4 también se compararon íntegramente **1.727.173.689 bytes**, que fueron idénticos. Los [registros independientes](docs/evidence/c72/independent-final-boundaries.json), las piezas y sus manifiestos quedan fijados en este repositorio. C70 sigue conservado como [referencia anterior](docs/evidence/c72/preserved-c70-before-c72.json).

En el checkpoint completo de C72, etapa 4 (**68.895.950.532** instrucciones), la espera del recolector terminó: el hilo principal de Zygote volvió a ejecutar y HeapTaskDaemon volvió a dormir. La [observación de solo lectura](docs/evidence/c72/gc-wait-released-stage4.json) está ligada al estado original `0c0a0aed62131eb50dbaf092e1c27ef34af1a10b3dc08578b0d8fa4e306e8d51`. Al cierre de la etapa 6, la lista verificada de Zygote contiene únicamente el hilo principal: el JIT que se ajustó ya no pertenece al grupo. C73 confirmó después `system_server`; C74 continúa con **cero entradas UART nuevas** hasta el arranque completo y la revisión de la interfaz. La lectura de los hilos reales de ART detectó que el comando de C71 devolvía éxito pero conservaba nice +19: en Toybox, `renice -n 0` suma cero. C71 se conserva como control. C72 corrige el comando a un incremento de −19, verifica primero la identidad y la prioridad reales del hilo JIT y conserva la entrada UART exacta. Las etapas solo continúan desde resultados plenamente admitidos y guardan cada checkpoint antes de avanzar.

El fotograma auténtico anterior de C69 mostró el **logo de inicio de Android en 800×600**. Se verificaron el ZIP, la pieza de transporte y el PPM original antes de inspeccionar la imagen. Esta observación no acredita el arranque completo.

![Fotograma original de C69: logo de Android](docs/evidence/c69-android-logo.png)

## Frontera C68 conservada como respaldo

| Comprobación | Resultado C68, etapa 12 |
|---|---|
| Frontera independiente verificada | 64.295.950.532 instrucciones |
| Checkpoint SHA-256 | `f306e9ef430186b8839a75a4e29c4f57cda106f9b958ab0c5db10670c246b18e` |
| RAM lógica | 2 GiB |
| Formato compacto y decodificador originales | Conservados |
| `system_server` | Pendiente de verificar |
| `sys.boot_completed=1` | Pendiente de verificar |
| Contenido Android visible | Pendiente de verificar |

C68 queda como respaldo. El trabajo continúa desde el último checkpoint plenamente admitido, sin reiniciar el arranque. Los resultados posteriores se publican como artefactos y en el resumen de cada ejecución. Un check de migración correcto acredita integridad y avance; el arranque completo exige las comprobaciones originales de Android.

## Compilar y comprobar

Se requiere Linux, GCC, Python 3.11+, Node 24 y OpenSSL. La dependencia Mundo X se fija en `e2778d962af22f6f1a0794058dd4685fc2573966`, sin modificarla:

```bash
git clone https://github.com/guadalupejuan001-code/mundo-X.git deps/mundo-x
git -C deps/mundo-x checkout --detach e2778d962af22f6f1a0794058dd4685fc2573966
bash scripts/build-reverseandroid.sh
```

`state/latest.json` registra el último resultado verificado. `state/continuation.json` fija ese checkpoint como origen del siguiente ciclo y su presupuesto. El flujo `reverseandroid.yml` autentica el estado completo, recupera el disco original y ejecuta el mismo motor. Cada resultado válido se conserva antes del siguiente paso. Los checkpoints se transportan en piezas de hasta 200 MiB, con manifiesto y SHA-256 por pieza, para permitir su recuperación con las herramientas actuales.

Con `execute_on_push: false`, guardar documentación o el nuevo checkpoint de referencia solo ejecuta la validación. El siguiente ciclo se solicita con `workflow_dispatch` o fijando `execute_on_push: true` en el plan. Siempre parte de los artefactos fijados por hash en `state/bootstrap-artifacts.json`; no vuelve automáticamente al checkpoint inicial.

Para recuperar un checkpoint fuera de Actions, reunir las piezas y su `manifest.json` en un directorio y ejecutar `python3 scripts/unpack-reverseandroid-checkpoint.py piezas destino-nuevo`. El proceso verifica las piezas, restaura los bytes originales y comprueba el hash completo. La admisión original del emulador sigue siendo obligatoria antes de ejecutar el estado.

Se preservan XI/Ether, fondo cero, RVG/MXIM/RFP, la RAM lógica, el disco, la semilla temporal, 64 instrucciones por tick, comprobación de IRQ y las condiciones originales de aceptación. La migración C69 no introdujo entradas UART ni cambios de prioridades. El experimento C72 registra explícitamente un único comando real de consola que cambia nice +19 a 0 para el hilo JIT identificado; su entrada exacta y la prioridad leída en el kernel están en `state/experiments/c72-plan.json` y `docs/evidence/c72/effective-priority-first-stage.json`. Las etapas posteriores continúan sin repetir esa entrada.
