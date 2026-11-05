# REVERSE-PC — XI Machine v2: contrato de ejecución programable

## Objetivo
Extender la máquina XI v1 fusionada en PR #16 sin crear una CPU x86 paralela ni duplicar la autoridad QuantumRuntime/InfiniteCore/InverseMemory. El objetivo no es simular una computadora mediante gráficos, sino ejecutar programas completos y medibles con la arquitectura existente.

## Prioridades de implementación

1. **ISA binaria versionada.** Definir un formato compacto de instrucciones con encabezado, versión, longitudes verificadas, tabla de constantes y saltos validados. Implementar ensamblador, desensamblador y ejecutor binario que preserve exactamente la semántica del perfil textual `#!reverse-pc-machine-v1`. Los programas antiguos siguen funcionando sin modificaciones.
2. **Memoria y registros.** Añadir palabras clásicas de 64 bits y registros persistentes opcionales con snapshots y rollback. Nunca interpretar un handle cuántico no determinista como byte clásico sin medición explícita. Mantener la regla de fondo y las excepciones de Inverse Ether Memory; no convertir direcciones lógicas en arrays densos.
3. **Entrada/salida controlada.** Definir puertos/capacidades explícitos para consola, archivos propios y reloj virtual determinista. Sin acceso irrestricto al anfitrión ni red por defecto. Las salidas de un programa fallido no deben confirmarse; para efectos externos no reversibles, utilizar preparación y confirmación al commit.
4. **Ejecución completa.** Presupuesto de pasos, instrucciones, memoria, tiempo y dispositivos. Diagnósticos claros y continuación reproducible. No afirmar multitarea ni SO hasta implementarlos.
5. **Benchmarks comparativos.** Algoritmos de enteros, búsqueda, recorridos, operaciones repetidas, datos dispersos y memoria por reglas. Comparar contra C++/intérpretes clásicos con la misma semántica y trabajo, en el mismo hardware. Medir tiempo, RSS, instrucciones, bytes físicos, I/O y corrección. Incluir casos donde XI sea peor.
6. **Prueba híbrida.** Un programa que calcule, almacene, ejecute Clifford+T en 128–1000 qubits estructurados, consulte observables, guarde, restaure y continúe. No atribuir aceleración clásica al número de qubits.

## Pruebas de aceptación
- Equivalencia textual/binaria, desensamblado y fuzzing de programas malformados.
- Pruebas de overflow, división por cero, saltos inválidos y bucles agotados.
- Restauración de estado y RNG, fallos inyectados, límites de recursos y no corrupción.
- CI Release + ASan/UBSan, contratos XI anteriores y campaña de 128/256/512/1000 qubits.
- Reporte de mediana/p95, memoria real y resultados crudos reproducibles.

## Restricciones
Conservar el checkout fijado de Mundo X y sus overlays comprobados por hash. No copiar SOVENGI, ChatGPT, Claude ni usar motores externos para fingir una CPU propia. No modificar el núcleo cuántico solo para mejorar un benchmark. PRs pequeñas y fusionar únicamente tras CI verde.

## Entrega
Empezar por el formato binario y su ejecutor funcional, no solo documentos. Después memoria persistente y E/S transaccional. Conservar el historial, documentar las limitaciones y dejar una demostración ejecutable.
