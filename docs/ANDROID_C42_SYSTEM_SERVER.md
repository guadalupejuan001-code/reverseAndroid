# C42 — completar Zygote y verificar system_server

La primera tanda usó C41: 19.316.342.731 instrucciones, SHA-256 `079cc299c91f95ac30f313c24aedb6cb8db84534256dbcf06687c83292eba1a0`, productor `0b1d2aec6613975fdca2ab88461b73df4b940be2`, artefacto 13113618839 del run 41381667947.

## Resultado de las etapas 1 a 8

El run 41385085636 publicó ocho checkpoints limpios después de comprobar igualdad de RVG con transacciones de 50.000 y 200.000 instrucciones. La etapa 8 alcanzó **23.317.342.731 instrucciones**, SHA-256 `b45add2d6796841bad15602a11184375148aa759efaf8faa5cc034c11d11dd2f`, productor `bac435fb89b1c689843bcc400c129942637a42b1`, artefacto **13118920257**, nombre `reverse-c42-stage-8-verified-checkpoint`.

La descarga completa, el preflight local y una nueva observación independiente del kernel coincidieron con el informe publicado. Zygote PID 567 sigue vivo y ejecutable; su CPU acumulada pasó de 1.566.692.000 a 3.162.886.000 ns. Se resolvieron las esperas puntuales de mmap y mprotect. **No se observó system_server**. Las ocho etapas pasaron sus comprobaciones; el run falló exclusivamente en la aserción final que exige ese hito. Esto no convierte las ocho salidas limpias publicadas en estados incompletos.

## Fuente de las etapas 9 a 16

El workflow actual continúa desde la etapa 8 fijada arriba. Autentica el contrato completo y sus hashes, productor y contador exactos antes de ejecutar. Conserva las observaciones anteriores con prefijo `previous-` y vuelve a leer el kernel y las propiedades del mismo RVG, sin cambiar ningún byte del invitado.

Las etapas nuevas aceptan hasta **1.000 millones de instrucciones y 15 minutos por etapa**. El argumento opcional del CLI conserva el valor anterior de 500 millones por defecto y rechaza valores que excedan el máximo nativo de 2.000 millones. Cada salida se autentica y publica antes de continuar.

La fuente de CPU, XI/Ether, RAM, disco, reloj y seguridad sigue siendo la de C41. Se compilan una referencia `-O2` y un candidato `-O3`, sin fast-math. El candidato debe pasar las regresiones de conversión escalar y FP/SIMD, tener los mismos pins y producir un RVG **idéntico byte a byte** al de la referencia después de un millón de instrucciones reales desde la misma etapa 8. La comparación incluye el cambio de transacciones 50k/200k. Solo entonces se publica y se usa el candidato. La compilación del observador de imagen mantiene `-O2`.

## Diagnóstico inicial observado

El PID 567 sigue siendo `/system/bin/app_process64 --zygote --start-system-server`, hijo de init, vivo y ejecutable. Su tiempo de CPU pasó de 1.158.009.000 ns en R2 a 1.566.692.000 ns en C41. Su pila mostraba `mas_store_prealloc`, `vms_gather_munmap_vmas`, `do_vmi_munmap` y `__arm64_sys_munmap`: hay actividad y cambio de trabajo, no prueba de un bloqueo fijo. No hay system_server ni socket Zygote receptivo verificados.

## Criterio de éxito

Un proceso `system_server` debe estar vivo conforme al estado real de task_struct, tener espacio de usuario, ejecutable `/system/bin/app_process64` comprobado desde el mm del invitado y padre igual a un Zygote vivo cuyo ejecutable y argumentos/nombre fueron comprobados. Se rechazan procesos muertos, detenidos, nombres de hilos de kernel, ejecutables distintos, padres incorrectos y ambigüedades. Esto no equivale a Android gráfico ni a `sys.boot_completed=1`.

## Procedimiento de la primera tanda

1. Autenticar y observar C41 sin escribir bytes del invitado.
2. Ejecutar 1.000.000 de instrucciones reales dos veces desde C41: transacciones de 50.000 y de 200.000. Exigir igualdad de los RVG completos y sus hashes. Solo la salida limpia e idéntica del segundo modo se publica y puede continuar.
3. Mantener la misma CPU C41, XI/Ether, RAM, reloj, disco, SELinux y odsign. Las opciones de host C39 de 200.000 instrucciones por transacción y checkpoints de 120 s/200 millones se utilizan después de la comparación; no modifican tiempo virtual ni estado arquitectónico.
4. Hasta ocho continuaciones separadas de hasta 500 millones de instrucciones y 15 minutos de CPU anfitriona por etapa. Después de cada una, autenticar contrato, procedencia, integridad, seguridad y progreso de Zygote; publicar el checkpoint y un informe pequeño antes de continuar.
5. Parar automáticamente cuando system_server esté verificado. Ante ISA desconocida, pánico, falta de progreso o fallo de contrato, no autorizar la salida: conservar la última etapa limpia publicada y el candidato fallido aparte como observación.

La cantidad de instrucciones no es el criterio de éxito. Si termina el presupuesto de etapas sin system_server verificado, la comprobación final falla explícitamente y conserva todos los checkpoints limpios publicados. Los informes distinguen CPU creciente, proceso ejecutable y espera de E/S; no presentan una espera puntual como prueba de deadlock.

Los diarios temporales de host se eliminan después de publicar el checkpoint. Se mantienen las últimas copias locales necesarias para continuar; las anteriores siguen disponibles en sus artefactos de GitHub durante 90 días. Los estados fallidos nunca se convierten en fuentes de continuación.

La rama sigue experimental y no modifica main.
