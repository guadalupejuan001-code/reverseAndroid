# reverseAndroid

Repositorio propio del **emulador Android de REVERSE**, con el modelo compacto XI/Ether original.

La base C68 completa procede de [REVERSE-PC, commit 37f5b461](https://github.com/guadalupejuan001-code/REVERSE-PC/tree/37f5b4610c5c31981e359fcf8fd0b303c14f01fd). Sus 870 archivos se conservan y se comprueban por hash en `state/upstream-snapshot.json`. Los flujos históricos están en `docs/upstream-workflows`; aquí solo se ejecutan los del emulador. La documentación anterior se conserva en `docs/REVERSE_PC_README.md`.

## Último resultado verificado

La época corregida C76/C77/C78 ha admitido **28.000.000.000 instrucciones**, conservando el disco efectivo del arranque anterior. Los límites completos de C78 están admitidos por el productor y conservan todas sus piezas. El snapshot completo de 24 mil millones coincide por SHA-256 con la ejecución local independiente. La recuperación local de los límites posteriores sigue pendiente; [estado y piezas](state/latest.json).

El servicio de luces **PID 652 está vivo y escucha realmente en el puerto VSOCK 6900**: se comprobaron su descriptor, los enlaces del socket y el estado `TCP_LISTEN` con el BTF del kernel original. [Evidencia auténtica](docs/evidence/c76/stage16-lights-vsock-readonly.json). SurfaceFlinger PID 669 está ejecutándose, SELinux permanece activo y el proceso PID 858 ya ejecuta `bootanimation`. La [lectura de los hilos en 27 mil millones](docs/evidence/c78/stage4-threads-summary.json) conserva sus identidades y estados reales.

El arranque completo y la interfaz siguen pendientes: todavía no aparece `system_server` y el fotograma auténtico de 800×600 sigue completamente negro. El [PPM original y su conversión PNG sin pérdida](docs/evidence/c78/local24-original-frame.json) están ligados por hash; los fotogramas hasta 28 mil millones son idénticos. Una [prueba aislada de consola](docs/evidence/c76/actual-console-response-control.json) recibió una respuesta generada por Android sin modificar el checkpoint principal. [C78 continúa la misma época](.github/workflows/android-c78-continued-epoch.yml), sin reiniciar la CPU.

El [ensayo aislado del compilador](.github/workflows/android-host-pgo-eval.yml) mide una compilación con perfil real desde una fuente fija: exige snapshots originales completos idénticos y una mejora repetida de al menos el 10 %. La candidata no es el ejecutable principal y sus estados no son autoridad de continuación.

[C75, etapa 6](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42098763084) conserva **`system_server` vivo, PID 3340**, a **88.895.950.532 instrucciones**, con checkpoint `7ecff7c2d460c582eeb3cbdf47fdd6686790cff2add75cd6c7437af7c9b9c3ec`. Pasó la [recuperación y admisión originales](docs/evidence/c75/independent-final-recovery.json). Android todavía no ha completado el arranque.

[C76](docs/c76-lights-boot-profile.md) investiga un fallo concreto del servicio de luces: falta su puerto VSOCK y el proceso aborta con permiso denegado. La corrección añade el puerto de arranque `6900`. Declara una nueva época de CPU y conserva **todo el disco efectivo de C75**, comprobado independientemente; mantiene kernel, CPIO, RAM, semilla, reloj y los 870 archivos originales. La nueva época debe volver a verificar init, arranque real, interfaz y respuesta a una entrada. No escribe propiedades Android en vivo.

[C74, etapa 6](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42088682642) verificó **`system_server` vivo, PID 3340**, en **82.895.950.532 instrucciones**. El checkpoint `a3d93b9a2a5665fc9bdbe09f5172cfbcd5314abdce82eb2cbb30557eb9442fd4` está conservado íntegramente en cuatro piezas. Los [seis snapshots completos](docs/evidence/c74/independent-final-boundaries.json) coinciden por SHA-256 con la ejecución local; también pasó la [admisión original de recuperación](docs/evidence/c74/independent-stage6-recovery-admission.json). SELinux sigue activo. El arranque completo y la interfaz continúan pendientes de verificar; C75 continuó desde esta frontera sin entradas UART nuevas.

La [lectura de los hilos](docs/evidence/c74/stage6-threads-summary.json) confirma que SystemServer pasó de 12 a 38 y después a **48 hilos**: aparecen ActivityManager, android.ui, android.display y PowerManager. La [última captura auténtica inspeccionada](docs/evidence/c74/stage6-original-scanout.png) sigue mostrando la animación de inicio. C73 queda conservado como [referencia anterior](docs/evidence/c73/independent-real-system-server.json).

[C72, etapas 1–6](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42062131978) completó su última frontera original en **70.895.950.532 instrucciones**, con checkpoint `18a5147749f4d13c776fee9bbb313fc20422357c46a9da6deaf2d8b450e1084b`. Los hashes de los seis snapshots completos coinciden con la ejecución local. En la etapa 4 también se compararon íntegramente **1.727.173.689 bytes**, que fueron idénticos. Los [registros independientes](docs/evidence/c72/independent-final-boundaries.json), las piezas y sus manifiestos quedan fijados en este repositorio. C70 sigue conservado como [referencia anterior](docs/evidence/c72/preserved-c70-before-c72.json).

En el checkpoint completo de C72, etapa 4 (**68.895.950.532** instrucciones), la espera del recolector terminó: el hilo principal de Zygote volvió a ejecutar y HeapTaskDaemon volvió a dormir. La [observación de solo lectura](docs/evidence/c72/gc-wait-released-stage4.json) está ligada al estado original `0c0a0aed62131eb50dbaf092e1c27ef34af1a10b3dc08578b0d8fa4e306e8d51`. Al cierre de la etapa 6, la lista verificada de Zygote contiene únicamente el hilo principal: el JIT que se ajustó ya no pertenece al grupo. C73 confirmó después `system_server`; C75 continúa con **cero entradas UART nuevas** hasta el arranque completo y la revisión de la interfaz. La lectura de los hilos reales de ART detectó que el comando de C71 devolvía éxito pero conservaba nice +19: en Toybox, `renice -n 0` suma cero. C71 se conserva como control. C72 corrige el comando a un incremento de −19, verifica primero la identidad y la prioridad reales del hilo JIT y conserva la entrada UART exacta. Las etapas solo continúan desde resultados plenamente admitidos y guardan cada checkpoint antes de avanzar.

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

C68 queda como respaldo. El trabajo conserva el linaje del último checkpoint plenamente admitido. Los cambios de perfil que exigen otro arranque se declaran como nueva época y conservan íntegramente el disco efectivo. Los resultados posteriores se publican como artefactos y en el resumen de cada ejecución. Un check de migración correcto acredita integridad y avance; el arranque completo exige las comprobaciones originales de Android.

## Compilar y comprobar

Se requiere Linux, GCC, Python 3.11+, Node 24 y OpenSSL. La dependencia Mundo X se fija en `e2778d962af22f6f1a0794058dd4685fc2573966`, sin modificarla:

```bash
git clone https://github.com/guadalupejuan001-code/mundo-X.git deps/mundo-x
git -C deps/mundo-x checkout --detach e2778d962af22f6f1a0794058dd4685fc2573966
bash scripts/build-reverseandroid.sh
bash scripts/build-reverseandroid-epoch.sh
```

`state/latest.json` registra el último resultado verificado. `state/continuation.json` identifica la época, el origen y el presupuesto. `reverseandroid.yml` conserva la validación del motor importado; el flujo de continuación de la época actual es `android-c78-continued-epoch.yml`. Cada resultado válido se conserva antes del siguiente paso. Los checkpoints se transportan en piezas de hasta 200 MiB, con manifiesto y SHA-256 por pieza.

Con `execute_on_push: false`, el flujo genérico no continúa el antiguo perfil C27. La continuación actual se ejecuta en su flujo específico, que recupera los artefactos fijados por hash y usa `build/reverseandroid/epoch`. El ejecutable histórico `native` conserva sus comprobaciones del perfil anterior. Los cambios de documentación no reinician el invitado.

Para recuperar un checkpoint fuera de Actions, reunir las piezas y su `manifest.json` en un directorio y ejecutar `python3 scripts/unpack-reverseandroid-checkpoint.py piezas destino-nuevo`. El proceso verifica las piezas, restaura los bytes originales y comprueba el hash completo. La admisión original del emulador sigue siendo obligatoria antes de ejecutar el estado.

Para un checkpoint de C76/C77, ejecutar `node --expose-gc tools/reverseandroid-epoch.mjs preflight destino-nuevo`. La continuación usa `tools/reverseandroid-epoch.mjs run`; conserva los decodificadores, las observaciones del kernel y las condiciones originales de arranque. El ejemplo completo está en [la documentación de la época](docs/c76-lights-boot-profile.md).

Se preservan XI/Ether, fondo cero, RVG/MXIM/RFP, la RAM lógica, el disco, la semilla temporal, 64 instrucciones por tick, comprobación de IRQ y las condiciones originales de aceptación. La migración C69 no introdujo entradas UART ni cambios de prioridades. El experimento C72 registra explícitamente un único comando real de consola que cambia nice +19 a 0 para el hilo JIT identificado; su entrada exacta y la prioridad leída en el kernel están en `state/experiments/c72-plan.json` y `docs/evidence/c72/effective-priority-first-stage.json`. Las etapas posteriores continúan sin repetir esa entrada.
