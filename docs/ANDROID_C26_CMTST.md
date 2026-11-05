# C26: continuar después de CMTST

La prueba C25 auténtica avanzó 2.004.350.743 instrucciones: completó las inicializaciones de `aconfigd`, ejecutó `post-fs` y llegó a `servicemanager`, `vold`, `system_suspend` y `keystore2`. Se detuvo en `recovery-refresh` (PID 395), ante `cmtst v7.16b, v3.16b, v3.16b`, opcode `0x4e238c67`, PC `0x74680de1e0`. No se observó un pánico ni reinicio Android.

C26 amplía el decodificador existente de CMEQ para la variante CMTST: cada posición queda a unos si los operandos comparten algún bit activo, o a cero si no lo comparten. Conserva las siete organizaciones vectoriales válidas, las trampas FP/SIMD y los flags. La organización de 64 bits con Q=0 sigue reservada.

Pruebas locales: 15.807 casos, 572.587 comprobaciones UBSan, 2.048 codificaciones reservadas rechazadas sin cambios y 458.752 codificaciones GNU. El oráculo prueba los bits individualmente e incluye la regresión CMEQ, valores no nulos sin bits comunes, ceros, bits altos, alias de registros, restauración y estado XI/Ether. Las 100 pruebas de host también pasan. CI repite el ensamblado con LLVM y la base del intérprete.

La fuente se fija al RVG C25 `bf73c3de585cfed346ddd5dfd8756f0b99ce1ccb4f2419cb637c247afbf7ac27`, 6.562.538.801 instrucciones y fase 13. El binario prueba la instrucción sobre una copia, contrasta su resultado con BigInt y exige que la fuente siga exactamente idéntica antes de continuar el original. Kernel, CPIO, AVB, SELinux, disco, RAM, reloj y núcleo XI/Ether conservan su perfil.

La prueba local de esa continuación pasó la comprobación de la instrucción auténtica y avanzó otras 52.800.000 instrucciones hasta 6.615.338.801. Terminó limpiamente por presupuesto del host; su checkpoint es `2e03e0aeb714368c5d8eb1e03c520d5b7bb333d9370fc5be722f5fff5750a6a8`.

`tools/android_c26_run.mjs` verifica la fuente, el descriptor compilado, supervisa el binario y publica el resultado real. El contrato C26 solo identifica esta parada de ISA. La herramienta genérica de presupuestos la rechaza; C26 usa la continuación específica después de reparar la instrucción.

La nueva ejecución larga publica observaciones fijadas del diario durante su primer minuto y a los quince minutos. El mismo proceso y CPU continúan ejecutándose. Una imagen opcional procede únicamente de la captura nativa del dispositivo y se etiqueta como una observación de la sesión, sin atribuirla al instante exacto del checkpoint. Las generaciones finales se conservan también ante un error.

El framework completo y una pantalla Android todavía requieren evidencia real. Ninguna prueba del intérprete o de transporte sustituye esa validación.

Referencia de instrucción: [Arm Instruction Set Reference Guide, CMTST](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab).
