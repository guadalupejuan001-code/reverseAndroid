# Separación dinámica de bloques — 25 octubre 2026

## Contrato

AdaptiveReduced conserva las puertas y el Reduced canónico originales. Después
 de una CX/CZ dentro de un bloque prueba particiones del árbol de fusiones y
particiones de un solo qubit. Una separación se acepta únicamente cuando
**cada amplitud compleja se reconstruye con igualdad exacta de double**, fase
incluida. No se aplica umbral ni se elimina un residuo pequeño. Los factores
conservan su norma conjunta; los observables incluyen las normas de los otros
bloques, igual que los observables no normalizados de Reduced.

La normalización usa el lado menor de la partición; certificados necesarios
sobre índices de soporte rechazan pronto muchos candidatos entrelazados. Las
asignaciones y la separación ocurren sobre un candidato privado antes de
confirmar el estado y el historial. Los cortes heredados del árbol forman una búsqueda acotada de
particiones candidatas por bloque. No se materializa el producto global para recuperar factores.

## Validación

- Las seis suites previas y las dos suites de la auditoría PR #5 se conservan.
  Los dos archivos `rigor_*` de PR #5 se recuperan sin cambiar sus pruebas;
  solo se concilian CMake y el workflow actual.
- Nueva suite: 400 circuitos aleatorios, 50 puertas más inversas por circuito,
  comparación compleja después de cada puerta, hasta seis qubits; error máximo
  observado 5.12e-15 frente al oráculo independiente.
- 1.000 qubits: CX/CX y CZ/CZ recuperan 1.000 bloques sin fallback; dos pares
  Bell recuperan su partición después de quitar el puente entre ellos.
- 67 estados con residuos no nulos menores que 1e-12 se conservan y se
  entrelazan con una CX; no se usa tolerancia para borrarlos.
- Nueve suites Release aprobadas. ASan/UBSan se ejecutan sobre siete suites
  de corrección, excluyendo dos perfiles de rendimiento de la instrumentación.
  LeakSanitizer queda activo en CI. En el contenedor local no puede leer
  `/proc/<pid>/task`, por lo que el pase local usa `detect_leaks=0`.

## Antes/después, mismo programa y servidor

21 procesos por caso, un calentamiento, orden alternado; tiempos incluyen
construcción y ejecución, sin oráculo denso. GCC 13.3, C++17 -O3. RSS es
`/proc/self/status` VmRSS/VmHWM después de exec, en KiB. También se conserva
ru_maxrss por transparencia, pero puede incluir el pico heredado del lanzador
antes de exec. Las capacidades de contenedores son otra métrica distinta.

| Circuito | Antes ms mediana / p95 | Después ms mediana / p95 | Pico RSS KiB antes / después |
| --- | ---: | ---: | ---: |
| uncompute | 1.427895 / 2.103778 | 0.039499 / 0.226364 | 2052 / 1772 |
| history | 1.848754 / 2.418698 | 1.888506 / 3.027694 | 3288 / 3288 |
| replay | 23.921865 / 28.295967 | 23.415352 / 33.145120 | 2744 / 2744 |
| entangled | 1.023213 / 2.465979 | 7.638597 / 10.657297 | 2136 / 2540 |
| random | 0.029791 / 0.055026 | 0.065450 / 0.140278 | 1756 / 1788 |
| product | 0.202651 / 0.682542 | 0.233583 / 0.441664 | 2040 / 2064 |

Datos agregados: [refactorization-local.json](measurements/refactorization-local.json).
CI publica las muestras completas con `scripts/compare-resources.py`. Su baseline
es `e8a04e029adbeae3fc9bd963374d93d61f24f524`, extraído sin modificarlo.

## Límites y siguiente paso

La detección tiene un coste real: el caso GHZ que alterna separaciones y
fusiones sigue siendo más lento que conservar un bloque grande. Este cambio
no promete acelerar todos los circuitos. La memoria de metadatos de cada
bloque añade un vector de cortes. Los certificados pueden rechazar un producto
por redondeo o no encontrar una partición que no esté entre las candidatas;
eso conserva el bloque y no cambia las amplitudes. Tampoco se intenta dividir
Reduced una vez activado.

El historial redundante y el replay permanecen medidos como problemas
separados. La siguiente iteración elimina cancelaciones demostrablemente
seguras, libera capacidades después de fallback y evalúa la importación de
productos con límites de componentes y rechazo atómico. La dependencia Mundo X
sigue fijada a e2778d962af22f6f1a0794058dd4685fc2573966.
