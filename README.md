# reverseAndroid

Repositorio propio del **emulador Android de REVERSE**, con el modelo compacto XI/Ether original.

La base C68 completa procede de [REVERSE-PC, commit 37f5b461](https://github.com/guadalupejuan001-code/REVERSE-PC/tree/37f5b4610c5c31981e359fcf8fd0b303c14f01fd). Sus 870 archivos se conservan y se comprueban por hash en `state/upstream-snapshot.json`. Los flujos históricos están en `docs/upstream-workflows`; aquí solo se ejecutan los del emulador. La documentación anterior se conserva en `docs/REVERSE_PC_README.md`.

## Último resultado verificado

[C69 ejecutado en reverseAndroid](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42050799049) completó **100.000.000 instrucciones nuevas** y dejó una frontera limpia en **64.395.950.532 instrucciones**. El checkpoint es `119be5c1aa583e7b6819f68157d94a0a6be0a44e6159d8e47005800e563ef6f4`.

El nuevo estado, sus tres piezas verificadas y el disco original ya están guardados en este repositorio. `state/latest.json` registra el resultado y `state/continuation.json` prepara C70 desde esa frontera. Todos los pines de entrada activos apuntan a artefactos de reverseAndroid. Los 870 archivos de la base importada conservan sus hashes originales; la compilación, el decodificador y las 95 pruebas de admisión pasaron. `system_server`, el arranque completo y contenido Android visible siguen pendientes.

El fotograma auténtico de C69 muestra el **logo de inicio de Android en 800×600**. Se verificaron el archivo ZIP, la pieza de transporte y el hash del PPM original antes de inspeccionar la imagen. Esta observación no cambia los controles originales de arranque completo.

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

El trabajo continúa desde esa frontera, sin reiniciar el arranque. Los resultados posteriores se publican como artefactos y en el resumen de cada ejecución. Un check de migración correcto acredita integridad y avance; el arranque completo exige las comprobaciones originales de Android.

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

Se preservan XI/Ether, fondo cero, RVG/MXIM/RFP, la RAM lógica, el disco, la semilla temporal, 64 instrucciones por tick, comprobación de IRQ y las condiciones originales de aceptación. Este ciclo de migración no introduce entradas UART ni cambios de prioridades en Android.
