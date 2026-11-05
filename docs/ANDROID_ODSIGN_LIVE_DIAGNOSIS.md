# Diagnóstico real de la barrera odsign -> Zygote

## Hallazgos reproducidos en C34
- Estado C34 SHA-256: `5e7a85b5e6b6ecd78140c95ad4be763c6a9b80bbd88c9ab41ce2b3124fc01b53`, 14,807,298,839 instrucciones; ejecución [41345457550](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41345457550) aprobada.
- `odsign` PID 554 permanece vivo pero duerme en `__arm64_sys_ioctl -> schedule_timeout` (espera de IPC compatible con Binder). `keystore2` y algunos procesos `android.hardware.*` muestran esperas similares.
- `init` PID 1 está en `epoll_pwait`. `zygote64`, `system_server`, `surfaceflinger` no están presentes.
- La evidencia no prueba por sí sola un error del driver Binder; el proceso podría estar esperando una respuesta aún pendiente de otro servicio.

## Dependencia AOSP real
Android init ejecuta `derive_classpath`, `art_boot`, inicia `odsign` y espera `odsign.key.done=1` antes de seguir. El trigger `zygote-start` espera `odsign.verification.done=1` antes de iniciar Zygote.
Fuente: https://android.googlesource.com/platform/system/core/+/master/rootdir/init.rc

## Investigación read-only
- El intérprete XI/Ether, kernel, discos, SELinux, PCI y snapshot permanecen intactos.
- `tools/android_boot_property.mjs` admite búsquedas por nombre exacto en el trie Bionic con protección de serial/offset, preservando la semántica anterior de `sys.boot_completed`.
- `tools/android_live_property.mjs` inspecciona mapeos compartidos *reales* de PID1 para `odsign.key.done`, `odsign.verification.done`, estado del servicio `odsign`, `keystore2`, KeyMint y `apexd.status`.
- CI toma el artefacto C34 autenticado, ejecuta pruebas de rechazo contra datos falsos, hace una segunda lectura del checkpoint y genera el diagnóstico con estado de los servicios y las trazas del kernel.
- Un valor de propiedad, aun autenticado, no basta para declarar Android iniciado ni un framebuffer correcto.

## Decisión siguiente
Si `odsign.key.done` y `odsign.verification.done` siguen ausentes, investigar interacciones Binder entre `odsign`, `keystore2` y KeyMint mediante estado de threads/colas, no falsificar propiedades. Si están en 1, analizar el verdadero bloqueador del trigger `zygote-start` (propiedades y servicios).
