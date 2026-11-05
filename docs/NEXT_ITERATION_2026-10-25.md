# REVERSE-PC: entrega integrada y siguiente iteración

## Estado actual

- PR #11: transferencia del estado actual de tableau/amplitudes al Reduced
  canónico, tensores y snapshots sin replay. Fusionada con CI verde.
- PR #12: una autoridad InfiniteCore con payload CompactCore/AdaptiveReduced/
  Reduced; XI T/T†, fases físicas bajo Ether, Born general, memoria enlazada,
  rollback conjunto y RVPC2 con revisiones/RNG. Fusionada con CI verde.
- La campaña posterior añade 60 circuitos completos de 128/256/512/1000 qubits,
  40 casos cortos bajo sanitizadores, comparaciones repetidas y observación
  paralela de solo lectura. Véanse [método y resultados](integrated-scale-campaign.md).
- El checkout Mundo X sigue en `e2778d962af22f6f1a0794058dd4685fc2573966`.
  Las extensiones verifican los encabezados y CI publica el diff compilado.
- Se conservan la auditoría recuperada de PR #5 y las pruebas históricas,
  incluidos 175 casos de equivalencia XI con el runtime original.
- El [muestreo Born](born-sampling.md) compara el umbral double por palabras,
  hasta 17 en el caso subnormal extremo, conservando el RNG histórico de 1/2.
  Elimina la cuantización de 53 bits y publica también su sobrecoste de CPU.
- La [certificación de cosets](unchanged-cosets.md) evita reconstruir un mapa
  que una Clifford deja idéntico. Compara 24 casos de 128–1000 qubits con
  256/65536 coeficientes, conserva la referencia y prueba 67 fallos de asignación.

El [contrato vigente](unified-authority.md) sustituye las propuestas de integración
que figuraban antes en este archivo. El historial sigue disponible en Git y en
[WORK_HANDOFF](WORK_HANDOFF_2026-10-25.md). XI ya no tiene un backend Reduced
separado de la sesión que usa Ether y memoria.

## Qué demuestran las pruebas grandes

Un programa XI crea y transforma más de 100 qubits, combina Clifford/T/T†,
observa mediante Ether y memoria, guarda, restaura y continúa. Un grafo de 1000
qubits presenta entropía 500 entre mitades antes de T, admite 16 T independientes
con 65536 coeficientes y rechaza la siguiente sin corrupción. La prueba CLI
persiste ese estado en un archivo RVPC2 de unos 18 MB y continúa ejecutándolo.

Los circuitos separados toleran miles de T al conservar factores locales. Son
una familia distinta. No se ha demostrado simulación eficiente de un circuito
genérico altamente entrelazado con un número arbitrario de T.

## Prioridad siguiente: inversas y estructura de Reduced

1. Perfilar las copias de tableau y cachés que quedan tras certificar cosets.
   A 1000 qubits, 96 CZ que conmutan aún realizan 384 asignaciones y unos
   24,7 MB de tráfico acumulado para esos candidatos. En una pareja completa,
   la inversión baja de 29,44 a 2,95 s, pero el pico RSS sube de 197 a 213 MiB.
   La inversión conserva residuos no nulos de redondeo; no resolverlos
   recortando amplitudes por tolerancia ni suponiendo ahorro global de RAM.
2. Ensayar un certificado algebraico de fases/inversas o coeficientes exactos
   para identidades Clifford+T. Demostrar que el ahorro no cambia fases,
   revisiones, RNG ni snapshots, y medir también el coste en circuitos sin
   cancelaciones. El certificado no debe convertirse en un historial ilimitado.
3. Detectar cortes separables y permitir partición/democión de Reduced grandes
   mediante pruebas exactas o certificados suficientes. Conservar un camino
   que rechace/no divida si no puede demostrar la separación. El GHZ que se
   fusiona y separa repetidamente debe seguir como caso negativo.
4. Evaluar claves Pauli y cachés de observación actuales por K y ancho. Cualquier
   caché debe ser derivada, acotada e invalidable; nunca una segunda autoridad.
   Comparar tiempos y RSS tanto con consultas repetidas como con mutación frecuente.

## Programabilidad y límites

- Añadir control clásico de flujo XI por etapas: etiquetas/saltos y operaciones
  sobre bytes deterministas, presupuesto de pasos y rollback de todo el programa.
  Un handle cuántico no se convierte en booleano mediante una medición implícita.
- Diseñar transferencia entre componentes que excedan el ancho Reduced de 1000,
  manteniendo el fondo Ether y las correlaciones. Rechazar mientras no exista
  una representación admitida: no dividir un estado entrelazado por conveniencia.
- Evaluar formato de transporte incremental antes de ampliar los topes actuales
  (núcleo mixto 64 MiB, contenedor CLI combinado 32 MiB). Los límites del dueño
  receptor deben seguir siendo autoritativos al restaurar.
- La observación paralela es opcional y acotada a 8 trabajadores. No habilitar
  escritores concurrentes sobre una sesión sin un contrato transaccional nuevo.

## Criterios de aceptación para cada cambio

- Oráculos densos independientes de pocos qubits, incluido fondo Bell y Ether;
  pruebas de fases, normas, ramas raras y probabilidades de ambos proyectores.
- Misma matriz grande, semilla, compilador y presupuestos; comparaciones
  alternadas con calentamiento, mediana/p95 y muestras crudas conservadas.
- RSS real y payload lógico por separado, más picos temporales, componentes,
  latencia de transferencia/fallback y rechazo atómico al exceder límites.
- Persistencia/continuación con RNG y revisiones, formatos anteriores,
  errores inyectados y memoria enlazada que no mida por leer.
- Todas las suites históricas y nuevas, ASan/UBSan y CI verdes antes de fusionar.
  Mantener en el informe los casos lentos y las regresiones observadas.

## Reproducir la siguiente sesión

```sh
bash scripts/setup-ether.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 scripts/integrated-matrix.py ./build/reverse_pc_integrated_large \
  --legacy ./build/reverse_pc_legacy_large --parallel ./build/reverse_pc_parallel_observation \
  --full --repeats 7 --checkpoint evidence/matrix-checkpoint.json > evidence/matrix.json
python3 tests/integrated_cli.py ./build/reverse_pc evidence/cli \
  ./build/reverse_pc_previous_cli --hard
```

El baseline se reconstruye desde Git; clonar con historial completo. Los JSON
locales documentados conservan su compilador, límites y hash de ejecutables;
CI vuelve a medir sobre su propio runner y publica resultados independientes.
