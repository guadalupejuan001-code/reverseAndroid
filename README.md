# reverseAndroid

Repositorio propio del **emulador Android de REVERSE**, con el modelo compacto XI/Ether original.

La base C68 completa procede de [REVERSE-PC, commit 37f5b461](https://github.com/guadalupejuan001-code/REVERSE-PC/tree/37f5b4610c5c31981e359fcf8fd0b303c14f01fd). Sus 870 archivos se conservan y se comprueban por hash en `state/upstream-snapshot.json`. Los flujos históricos están en `docs/upstream-workflows`; aquí solo se ejecutan los del emulador. La documentación anterior se conserva en `docs/REVERSE_PC_README.md`.

## Estado de partida

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

`state/continuation.json` describe el checkpoint y el presupuesto del siguiente ciclo. El flujo `reverseandroid.yml` autentica el estado completo, recupera el disco original y ejecuta el mismo motor. Cada resultado válido se conserva antes del siguiente paso. Los checkpoints se transportan en piezas de hasta 200 MiB, con manifiesto y SHA-256 por pieza, para permitir su recuperación con las herramientas actuales.

Se preservan XI/Ether, fondo cero, RVG/MXIM/RFP, la RAM lógica, el disco, la semilla temporal, 64 instrucciones por tick, comprobación de IRQ y las condiciones originales de aceptación. Este ciclo de migración no introduce entradas UART ni cambios de prioridades en Android.
