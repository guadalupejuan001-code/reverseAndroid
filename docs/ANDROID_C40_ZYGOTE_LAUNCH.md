# Android C40 — prueba real de Zygote desde C39

## Descubrimiento (memoria real, no simulación)
El checkpoint C39 de REVERSE-PC, verificado con SHA-256:

`094d5c9b9532fbb21ea029031b952f46c4df0e589c2f0d75a4dd64677bc49438`

tiene 18,393,448,839 instrucciones y contiene el proceso PID 567 llamado `app_process64`. La lectura **read-only** del `mm_struct` y de su pagetable de usuario, usando únicamente el GKI verdadero fijado por BTF, permitió recuperar y contrastar lo siguiente:

- Ejecutable real: `/system/bin/app_process64`
- PID del padre: **1** (`init`)
- Argumentos reales: `/system/bin/app_process64 -Xzygote /system/bin --zygote --start-system-server --socket-name=zygote`
- Estado del proceso: activo, con espacio de direcciones de usuario válido, en la cola del planificador.
- Kernel: el PID 567 no se llama todavía `zygote64`; `system_server` y `surfaceflinger` no se observaron en esa instantánea.

Conclusión legítima: Android **ha ejecutado el lanzador de Zygote**. No hay prueba de un socket Zygote ya operativo, de aceptación de conexiones ni de Android completamente iniciado. Buscar únicamente un proceso cuyo nombre sea `zygote64` ocultaba el hito de lanzamiento.

## Cambios C40
- En `tools/android_kernel_observer.mjs` se añade la inspección optativa `--zygote-probe`: reconoce ejecutables y argumentos de procesos vivos con offsets de `task_struct` y `mm_struct` BTF, lectura desde las tablas del propio proceso y comprobación de memoria/padres. La imagen RVG es estrictamente de solo lectura y se verifica su SHA original.
- Se amplía la observación exacta de propiedades de PID1 para `init.svc.zygote` y `init.svc.zygote_secondary`, sin modificar su valor.
- El clasificador `tools/android_c40_zygote_stage.mjs` distingue `zygote_launcher_executing`, `zygote_process_renamed`, `system_server_process_observed` y ausencia de prueba, sin confundirlos con `sys.boot_completed`. Todos los casos conservan `zygote_socket_responsive_verified=false` hasta disponer de una prueba distinta.
- Suite de pruebas negativas/positivas: lectura de argv, estados de salida, PID, rutas, discrepancia de SHA, falta de mmap, propiedades contradictorias.

## Ejecución auténtica
Workflow `.github/workflows/android-c40-zygote.yml`:

1. Descarga y reautentica el checkpoint C39 desde el run **41360328706** y el contrato original.
2. Comprueba el arranque inicial de `app_process64 --zygote` mediante lectura de RAM y página de usuario del propio invitado (sin activar CPU).
3. Compila la CPU XI/Ether C39 sin alterar el kernel, el disco, la firma AVB, el reloj 64 instrucciones por tick ni SELinux.
4. Reanuda **la misma máquina** desde el checkpoint original con tres presupuestos acotados de hasta 500 millones de instrucciones cada uno, protegidos por límites de tiempo y snapshots del trabajo genuino.
5. Vuelve a observar el nuevo checkpoint y las propiedades `odsign.key.done` / `odsign.verification.done`, y detecta si el PID del lanzador pasa a `zygote64` o aparece un `system_server` vivo.
6. Si el invitado alcanza una parada de arquitectura, reinicio, pánico o fallo de integridad, no afirma éxito ni promueve el estado; conserva los diagnósticos para resolver el error.

El éxito de CI confirma integridad y avance nativo, **no** un Android ya iniciando aplicaciones ni un APK funcional. Es una PR experimental apilada sobre C39, sin fusionar a `main`.
