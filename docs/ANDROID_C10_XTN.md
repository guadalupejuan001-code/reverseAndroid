# C10: XTN/XTN2 y continuidad exacta desde C09

Base: e62ac0d043bd5d88e122942e10d4128672458ee1, PR 77. No reiniciar C06/C07/C08.

El cambio del procesador implementa XTN/XTN2 en el intérprete existente: 16→8,
32→16 y 64→32 bits, truncamiento sin saturación, XTN borra la mitad alta,
XTN2 conserva la mitad baja, origen capturado antes de escribir para Rd==Rn,
size=11 rechazado y comprobación FP original. No cambia XI/Ether, memoria,
CPU alternativa, imágenes Android ni estado guardado.

Se conservan byte por byte las protecciones R2 de reanudación, supervisor,
auditor interno FNV/imagen/disco/reloj y sus 153 pruebas. El nuevo auditor
vincula C09 con C08/C07/C06 y usa el mismo validador de estado R2. El supervisor
recibe el contador inicial verdadero de C09 y el motor emite sus marcadores.

## Fuente y estado
- C09: run 40997177448, artifact 12951339528.
- Estado SHA-256: 773277104d567e2f72a6e1c66907dfcf5f87f17d77ee1b2dafcee64e87ab0ad3.
- Informe SHA-256: 2f50ba79ef1bcfd0838a3bbd344b92d749576671cd6f76e357159c91c73a6d0c.
- Contador: 6456253329; PC: 0x4ce520; opcode: 0x0e612804; EL0; fase 49/64.
- Disco original: run 40910284394, artifact reverse-android-c03-genuine-partition-disk.
- Disco: 8738832384 bytes, SHA-256 28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c.

## Pruebas locales iniciales
257105 comprobaciones en 7075 casos XTN, 6144 codificaciones LLVM, 11675
FP/SIMD, 70675 ampliación, 74 ciclo/diario, 153 R2 y 14 auditor C10.
El intérprete C09 anterior rechaza la nueva regresión. No son arranques Android.

## Ejecución y publicación
El flujo aplica el parche cuyo hash está fijado, compila y repite las pruebas.
Solo después publica esos mismos archivos de fuente en esta rama experimental
(sin force-push ni modificación de main). Después descarga el disco original y
reanuda C09 supervisado. Conserva código, binario, diarios y evidencias incluso
si aparece otro fallo. El resultado real se lee de milestone.json y del estado
final, nunca del número de pruebas ni de la conclusión general del pipeline.

La pantalla, el framework completo y los juegos no se afirman sin evidencia.
