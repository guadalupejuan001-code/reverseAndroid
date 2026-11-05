# C41 — instrucción real que detuvo C40 y C40R3

La recuperación C40R del run 41375758933 validó y publicó R1 (18.643.448.839 instrucciones) y R2 (18.893.448.839). R3 se detuvo exactamente en 19.066.342.718, igual que C40: `Unsupported AArch64 instruction 0x5ee1b801 at 0x7c9aaeb1cc`. El diario verificó SHA-256, no hubo watchdog, señal, pánico ni reinicio; el intérprete devolvió 2 por `architectural_stop`.

El opcode es **FCVTZS D1,D0**, conversión de punto flotante a entero con truncamiento hacia cero y destino SIMD. La primera ejecución C41 (run 41379891261) superó esta instrucción y se detuvo doce instrucciones después, en 19.066.342.730: `0x5e61d821` a PC `0x7c9aaeb1fc`, **SCVTF D1,D1**, la conversión inversa. El diagnóstico ya conservó correctamente la categoría de error y el binario fallido como observación.

Existían ambas conversiones a/desde registro general mediante `ArmFloat::to_int` y `ArmFloat::from_int`; faltaban sus variantes escalares AdvSIMD. El decodificador ahora reconoce FCVTZS/FCVTZU y SCVTF/UCVTF S/D, reutiliza la aritmética SoftFloat fijada, verifica acceso FP y borra la parte superior del destino. SCVTF/UCVTF respeta el redondeo FPCR; FCVTZS/FCVTZU trunca hacia cero. No cambia el reloj, la memoria, XI/Ether, el formato RVG, el disco ni la seguridad Android.

Referencia primaria: Arm A64 ISA DDI 0602, marzo de 2025, FCVTZS (vector, integer), páginas 1330–1331; SCVTF (vector, integer), páginas 1722–1723: https://documentation-service.arm.com/static/67e40f3398aa3c3b6eea6a85.

La prueba nativa reprodujo el rechazo de cada opcode exacto antes de su respectiva corrección. La suite completa verifica **8.762 casos / 316.267 comprobaciones**, en Release y UBSan: todas las parejas de registros, alias, anchos de 32/64 bits, variantes con/sin signo, ceros, fracciones, infinitos, NaN, saturación, FPSR acumulativo, redondeo FPCR en ambas direcciones, subnormales, restauración y trampas de acceso FP. Los resultados se comprueban con constantes especificadas independientemente, sin usar la función de conversión como oráculo.

La nueva ejecución parte **solo del R2 limpio**: SHA-256 `56821dfcec37af1031221247d8dd42f894e176c67fbf8ea3e87b3e14894d36a8`, contrato de cuatro archivos, productor `d8e5c796a6a40e716ef22f24f90185252f55558b`. No se promueve ni utiliza ningún estado fallido. El primer presupuesto ejecuta 172.893.892 instrucciones y exige un checkpoint limpio exactamente en **19.066.342.731**, después de ambas instrucciones reparadas. Se autentica, se comprueban odsign y SELinux y se publica **antes** de continuar otros 250 millones de instrucciones. Si aparece otro bloqueo, se conserva el checkpoint limpio ya publicado y el candidato fallido aparte como observación.

El arranque completo de Zygote y Android requiere evidencia posterior; una prueba nativa o un proceso anfitrión aprobado no lo demuestra.
