# C24: reanudar el punto auténtico de aconfigd

C23 completó `linkerconfig` y llegó a `early_system_aconfigd_platform_init`, pero el intérprete se detuvo ante `0x2e261066` en `0x5565cc14ac`. GNU objdump identifica `uaddw v6.8h, v3.8h, v6.8b`; el destino coincide con el segundo operando. No hubo pánico, reinicio ni fallo terminal del invitado.

C24 implementa SADDL/UADDL, SADDW/UADDW, SSUBL/USUBL y SSUBW/USUBW, incluidas sus variantes `2`. Copia ambos operandos antes de escribir el destino, rechaza `size=3`, respeta la trampa de acceso FP/SIMD y conserva NZCV, FPCR y FPSR. Q selecciona únicamente la mitad de los operandos estrechos; el resultado siempre ocupa 128 bits. Se usa aritmética modular sin desbordamiento con signo de C++.

La prueba independiente emplea enteros de 128 bits, 53.617 casos válidos, las 48 formas, 16.384 codificaciones reservadas, alias, bordes de signo y acarreo, restauración y estado XI/Ether. UBSan pasa 1.951.111 comprobaciones. GNU as confirma otras 189.024 codificaciones. La CI repite el ensamblado con LLVM y comprueba las instrucciones de multiplicación previamente implementadas.

La continuación fija el RVG C23 `8dfee020d3a5623fa1a1957567e5046637ef40b10ff378604569516c9471382a`, su informe, el disco de 17.330.864.128 bytes y la semilla del reloj. El descriptor del binario compilado se contrasta con constantes independientes antes de abrir el disco. Después ejecuta la instrucción sobre una copia, verifica su resultado con BigInt en el auditor y comprueba que el RVG original sigue idéntico. El flujo principal empieza desde el original; no omite instrucciones ni modifica registros, RAM, disco o reloj.

Se mantienen el perfil C21 de 2 GiB, AVB, SELinux y el transporte explícito de 1 GiB externo al formato RVG. El límite predeterminado sigue en 256 MiB. El trabajo ejecuta hasta tres presupuestos consecutivos de 15 minutos, con diarios atómicos y captura únicamente de imágenes realmente emitidas por el dispositivo gráfico. Un arranque completo del framework y una pantalla Android siguen requiriendo evidencia real de ejecución.

Referencia de semántica: [Arm Instruction Set Reference Guide, UADDW/UADDW2](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab).
