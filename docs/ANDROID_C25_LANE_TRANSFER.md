# C25: transferencias SIMD de una posición

C24 verificó y ejecutó la instrucción UADDW auténtica, conservó el origen intacto y avanzó 13 instrucciones. El siguiente opcode no implementado es `0x0d9f8186`, `st1 {v6.s}[0], [x12], #4`, en PC `0x5565cc14e0`. No se observó un pánico ni un reinicio Android.

La corrección implementa LD1/2/3/4 y ST1/2/3/4 de una sola posición por registro. Respeta los tamaños 8/16/32/64, todas las posiciones válidas, las listas que cruzan V31, SP y las tres formas de dirección. Captura los operandos y precomprueba todos los accesos antes de escribir. Las cargas conservan las demás posiciones del vector y las escrituras conservan los vectores. Rechaza las codificaciones reservadas y mantiene las trampas FP/SIMD. Los replicados ya existentes se decodifican por separado.

Validación local: 70.560 casos, 5.527.497 comprobaciones UBSan y 737.280 codificaciones ensambladas independientemente con GNU as. Incluye guardas de memoria, orden de bytes, alias del registro base y desplazamiento, fallo del último elemento, restauración y preservación de flags/core XI/Ether. La CI repite las codificaciones con LLVM y la regresión de cargas replicadas.

El punto de entrada es el RVG `e75a8c99db489a69b0e2c9f7cc067f31535ad638ffbc14a85a924dc300d42a7a`, con 4.558.188.058 instrucciones y fase de reloj 23. Las huellas del binario, informe, disco y semilla se comprueban independientemente. Antes de continuar se prueba ST1 sobre una copia, verificando el valor de 32 bits escrito y el incremento de X12; el estado de entrada debe permanecer idéntico. No se omite la instrucción ni se reinicia la CPU.

AVB, SELinux, kernel, CPIO, RAM, disco, reloj y núcleo XI/Ether conservan sus identidades. El arranque completo de Android y una pantalla siguen pendientes de evidencia real.

Referencia: [Arm A64, LD/ST single structure](https://documentation-service.arm.com/static/67e40f3398aa3c3b6eea6a85).
