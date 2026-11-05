# Transferencias de estado actual, primera etapa de integración

El checkout fijado conserva e2778d962af22f6f1a0794058dd4685fc2573966.
La extensión de compilación verificada incorpora métodos al Reduced canónico,
no un simulador ajeno ni otro registro sincronizado. Importa un CompactCore
actual, tensores de estados y vectores **locales** de hasta 14 qubits, sin
reproducir instrucciones anteriores. La base y los coeficientes actuales son
la autoridad; una caché de eliminación es de solo lectura e invalidable.

Las puertas Clifford conjugan la base y sus etiquetas Pauli sin reconstruir
una eliminación gaussiana por puerta. La expansión no-Clifford y las consultas
canonicalizan frente a esa base. Una rotación sobre un Pauli Hermitiano admite
la conjugación por Ether. Solo se eliminan coeficientes exactamente cero; los
residuos no nulos no se podan. La expansión usa scratch acotado a dos veces los
componentes anteriores y comprueba el presupuesto final antes de confirmar.

El snapshot RDX1 contiene la base estabilizadora y coeficientes actuales
ordenados, dimensiones y contador T; no requiere historial. Usa doubles IEEE
754 con bytes little endian y checksum FNV para corrupción accidental. El
receptor impone sus límites y valida tableau, padding, cosets, duplicados y
valores finitos. El RNG y las revisiones pertenecen al dueño InfiniteCore y se
integrarán en su sobre, no dentro de este payload local.

La reconstrucción densa local deriva amplitudes del tableau validado por
eliminación binaria y proyectores estabilizadores. Está limitada a 14 qubits y
solo se usa para transferencias pequeñas/oráculos; nunca expande 2^128 o 2^1000.
Las bases estabilizadoras, como CompactCore original, fijan una fase global
convencional. Se conservan las fases relativas y observables físicos.

Pruebas: 160 circuitos aleatorios de 2–8 qubits, con 50 puertas Clifford antes
de importar y 35 Clifford/T/T† después, contra un vector independiente;
GHZ de 128/256/512/1000 qubits con T/T†, correlaciones de alcance completo,
tensores que cruzan palabras de 64 bits, Born no 1/2, snapshots y rechazo
atómico al exceder componentes. La ruta XI se conecta en la siguiente etapa;
esta etapa no declara esa conexión ya realizada.
