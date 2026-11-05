# C72: prioridad de ART comprobada en el kernel

El control C70 terminó 1.000.000.000 instrucciones nuevas con admisión original completa en 65.395.950.532, SHA-256 `3811f9d7b3c79311318b415af958546712e31b78579ffb680cad595be4780bc4`. Zygote no avanzó y system_server no estaba verificado.

La lectura BTF de listas de hilos verificadas mostró a main esperando el GC completo y a HeapTaskDaemon dentro de una barrera de ART. Jit thread pool, TID 994 del proceso 567, estaba ejecutable con prioridad estática 139 (nice +19). La lectura adicional mantiene intactos el observador original y todos los bytes del checkpoint, y usa únicamente símbolos ELF con rangos declarados.

C71 solicitó `renice -n 0`; el comando devolvió cero, pero la lectura del kernel siguió mostrando nice +19. La ayuda oficial de Toybox describe `-n` como un incremento: https://landley.net/toybox/help.html#renice . Esa prueba se conserva como control sin cambio efectivo de prioridad. Su primer checkpoint fue reproducido independientemente y publicado antes de retirar los presupuestos posteriores de esa campaña.

C72 parte del C70 auténtico, comprueba otra vez la identidad y nice +19 del TID 994 y entrega exactamente 47 bytes por la operación PL011 modelada existente: interrupción de la consulta de consola anterior y `su 0 /system/bin/toybox renice -n -19 -p 994`. El incremento solicitado es −19, para alcanzar nice 0. El resultado efectivo se leerá en el kernel; una salida cero por sí sola no lo acredita.

Es una prueba temporal y explícita dentro del Android emulado. No modifica el estado guardado para fabricar una respuesta, la CPU, la autoridad XI/Ether, los formatos, el disco, la RAM lógica, la semilla temporal, las interrupciones ni las condiciones originales de arranque. El ajuste de prioridad no acredita por sí mismo un Android funcional: siguen siendo necesarios system_server, la propiedad de arranque viva, SELinux y la salida real de pantalla.
