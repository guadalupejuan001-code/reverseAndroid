# Android C39: bloques ARM64 predecodificados y optimización de XI/Ether

## Estado anterior comprobado
C38 (run 41355448713) terminó con 18,392,448,839 instrucciones y checkpoint SHA-256 fc0959a4f6f7dbfdc2a525dceae8346fea7b3215c40b01d15b0c39f6c37e5701. Las propiedades odsign.key.done y odsign.verification.done valen 1. No se observa todavía zygote64 ni una pantalla Android. Una ejecución CI correcta no equivale a boot gráfico.

## Arquitectura experimental C39
1. **Bloques de código ARM64:** 512 bloques físicos, de 64 bytes y 16 instrucciones cada uno. El intérprete decodifica cada opcode cuando se ejecuta por primera vez y reutiliza su clasificación para instrucciones frecuentes: MOV, ADD/SUB, aritmética lógica, ADR, saltos y multiplicación. No se preejecutan instrucciones; cada instrucción consulta traducción MMU/privilegios e interrupciones como antes. Instrucciones especiales, instrucciones SIMD, accesos a memoria, excepciones y dispositivos siguen usando el intérprete original.
2. **Lecturas XI/Ether:** caché no persistente de 2,048 bytes indexados por dirección física, con invalidación en escrituras CPU, pares atómicos, DMA, transiciones de memoria XI/RXE y dispositivos. Se desactiva cuando hay ligaduras cuánticas, preservando el único QuantumRuntime/InverseMemory y sin añadir otro almacén autoritativo.
3. **Agrupación del trabajo y checkpoints:** la política preexistente usa transacciones de 50,000 instrucciones y checkpoints cada 60 s / 100 millones de instrucciones. Tres parámetros de host optativos habilitan REVERSE_PC_C39_CHUNK=200000, REVERSE_PC_C39_CHECKPOINT_MS=120000 y REVERSE_PC_C39_CHECKPOINT_INSTRUCTIONS=200000000. No modifican el tiempo virtual Android ni el presupuesto total. Se conserva la política de seguridad anterior por defecto.

## Criterios comprobables
- Compilar el mismo código en referencia (sin decodificación rápida ni caché de datos) y optimizado.
- Ejecutar un programa real ARM64 con ALU, salto, lectura/escritura Ether, código automodificable y restauración.
- Compilar pruebas EL0, GKI, PCI/virtio y del sistema.
- Partir del checkpoint C38 SHA citado, del mismo disco Android genuino, y ejecutar exactamente 1,000,000 instrucciones por modo: referencia/50k, optimizado/50k y optimizado/200k.
- Exigir igualdad byte por byte de tres checkpoints RVG completos, hashes y procedencia exactos antes de admitir la optimización en Android.
- Registrar costes de host reales; no atribuir la mejora medida en un bucle artificial al arranque de Android.

## Límites
Esta implementación reutiliza bloques *interpretados*, no traduce a instrucciones nativas del procesador anfitrión mediante JIT. No omite SELinux, AVB, odsign, KeyMint ni suprime la verificación de Zygote. No modificar main hasta disponer de pruebas de compatibilidad y resultados reales.
