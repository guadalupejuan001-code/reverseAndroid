# XI integrado: campaña de 128 a 1000 qubits

## Alcance y reproducción

La ruta medida es QuantumRuntime → InfiniteCore → QuantumBlock, la misma que
usa el ejecutable XI. Se ejecutan puertas, consultas, memoria, serialización,
restauración, RNG e inversas en cada tamaño. No basta con materializar direcciones.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
python3 scripts/integrated-matrix.py ./build/reverse_pc_integrated_large \
  --legacy ./build/reverse_pc_legacy_large \
  --parallel ./build/reverse_pc_parallel_observation \
  --full --repeats 7 --checkpoint evidence/integrated-matrix-checkpoint.json \
  > evidence/integrated-matrix.json
python3 tests/integrated_cli.py ./build/reverse_pc output \
  ./build/reverse_pc_previous_cli --hard
```

CMake reconstruye el `main` anterior `0d8926bd65b01877eeef6e3172259284f8944a88`
con su código y extensiones verificadas reales. Ese baseline conserva la
optimización de transacciones privadas. Ambos probes grandes comparten fuente,
compilador y opciones `-O3 -DNDEBUG -std=c++17`; la precedencia de includes
selecciona la implementación. Se necesita historial Git completo para reconstruirlo.

El baseline anterior solo admite Clifford en XI. Las comparaciones no-Clifford
usan el mismo propietario integrado y los mismos circuitos, alternando una
optimización cada vez: `fast_reduced_clifford`, `direct_ether_phases` o
`factor_pair_shortcuts`. No se inventa una ejecución XI no-Clifford del main antiguo.

## Familias y comprobaciones

Hay 60 casos completos: 15 por tamaño. Las comparaciones añaden calentamientos
con verificación completa y siete muestras por modo, en orden alternado.
La matriz corta de CTest conserva 40 casos y también se ejecuta con sanitizadores.

| Familia | Trabajo real y comprobación |
| --- | --- |
| Clifford profundo | H/S/CX por capa, profundidades 8 y 32; hasta 96000 puertas a 1000 qubits; tableau de referencia, Paulis distantes y estabilizadores |
| Clifford aleatorio | 3·n·8 puertas H/S/CX/CZ, semilla 20261025, oracle CompactCore separado |
| GHZ | Cadena CX de todo el registro, 16 y 64 T en posiciones distantes; ZZ remoto y X global con fase analítica |
| Separable | H y 4/8 rondas de T/S/X en todos los qubits; hasta 8000 T a 1000 qubits, oracle independiente de un qubit por factor |
| Parcial | Grupos GHZ de ocho, T por grupo y CX/CX entre extremos del registro; inversión y restauración |
| Producto con enlaces redundantes | Control de base y blancos no-Clifford; identidad exacta de autoestados evita fusionar factores innecesariamente |
| Grafo mixto | CZ de línea y entre mitades; 6 T independientes y 48/96 Clifford posteriores; inversas y norma |
| Ether activo | Grafo, CZNeighbors/HAll y 8 T físicas; estabilizadores transformados Zq·Xpartner, persistencia e inversión del marco |
| Adversarial | Mismo grafo, T independientes hasta presupuestos 16/256/4096/65536; se admiten 4/8/12/16 T y se rechaza la siguiente atómicamente |

El grafo tiene entropía estabilizadora n/2 entre mitades **antes de T**, calculada
por rango GF(2). Se reporta esa etapa explícitamente: no se aplica la fórmula de
estabilizadores a un estado posterior no-Clifford. El GHZ tiene entropía 1;
los productos, 0. Un reparto de grupos entrelazados puede tener entropía 0 para
ese corte concreto sin ser un producto de qubits individuales.

Además de estos invariantes grandes, 200 circuitos Clifford+T aleatorios de
2–8 qubits y 32 ventanas Ether se comparan con vectores densos independientes.
Se prueban ambas políticas de bloques locales, ambas rutas de fase Ether y
los fondos Zero/BellPairs en las ventanas densas.
La prueba numérica de Born conserva una rama de probabilidad 1e-300.
CompactCore como oracle de Clifford comprueba el montaje y las transferencias;
no es una implementación independiente de su propio algoritmo de tableau.

Todos los casos completos guardan y restauran núcleo/memoria con revisiones,
continúan operaciones Ether, comparan RNG y aplican las inversas. Se comprueban
q0/q127/q255/q511/q999 cuando existen, junto con los observables propios de
cada familia. Una medición adicional que exige duplicar los 256 coeficientes
del caso Ether se rechaza, incluyendo rollback de memoria, revisión y RNG.
No se reconstruye un vector global de amplitudes.

## Cómo interpretar las medidas

Esta sección conserva las mediciones de PR #13,
`6b8cecc9345db8c56c9034e62b164c93f8afcffd`. La
[certificación posterior de cosets](unchanged-cosets.md) tiene su comparación
independiente, incluida una pareja completa de inversión con el mismo programa.

Los tiempos internos usan `steady_clock`. La fase `noncliff_ms` del adversarial
incluye snapshots de verificación antes de cada intento; `T_gate_ms` aísla cada
T admitida y `T_rejection_ms` el intento rechazado. Se publican ambos, junto con
observación, guardado, restauración e inversión.

RSS/HWM son lecturas reales de `/proc/self/status`, en KiB, por proceso nuevo.
`execution_*` se toma tras ejecutar el circuito y antes de validar, e incluye el
tableau oracle y las listas de puertas. `rss_kib`/`peak_rss_kib` también incluyen
snapshots, copias de comprobación, medición de ramas e inversas. El payload
lógico estima solo el estado de los componentes: excluye tablas de direcciones,
metadatos, cachés, asignador y temporales. No equivale a RAM física.

Las muestras repetidas conservan las comprobaciones hacia delante, pero omiten
repetir restauración/inversa: ya se verifican en los calentamientos y en la
matriz completa. Los intervalos de tiempo medidos son los mismos. p95 es el
percentil empírico por rango más cercano; con siete muestras es descriptivo,
no una garantía de latencia ni un intervalo de confianza.

Medición local del 25-10-2026: GCC 13.3, Linux x86_64, cgroup de 8 CPU y 8 GiB.
[Muestras crudas y hashes](measurements/integrated-matrix-local.json). Son
ejecuciones únicas para la matriz y siete muestras por modo para comparaciones.
El mayor error observable de los 60 casos fue 2.22e-16; la mayor desviación de norma fue 2.22e-13.

### Grafo adversarial, presupuesto 65536

| Qubits | T admitidas | Σ tiempo T admitidas (ms) | Rechazo T17 (ms) | Guardar / restaurar (ms) | Inversa (s) | RSS tras ejecutar / pico total (MiB) | Payload (MB) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 128 | 16 | 71.43 | 106.93 | 87.94 / 122.04 | 4.76 | 47.65 / 160.18 | 17.83 |
| 256 | 16 | 77.74 | 123.01 | 96.63 / 151.61 | 8.42 | 55.56 / 147.48 | 17.84 |
| 512 | 16 | 73.86 | 94.53 | 152.64 / 147.76 | 22.55 | 51.10 / 163.96 | 17.89 |
| 1000 | 16 | 122.42 | 155.44 | 286.05 / 164.07 | 33.17 | 60.57 / 197.08 | 18.08 |

La fase adversarial total incluye verificaciones: a 1000 qubits es 1439.99 ms,
frente a 122.42 ms de T admitidas. La diferencia incluye checkpoints, serialización
y rechazo; no se atribuye toda esa fase al cálculo de las puertas.
El pico de la inversión muestra un coste pendiente, aunque la norma y los
observables verificados permanezcan correctos.

### Optimizaciones aisladas: mediana antes → después (ms)

| Qubits | 96 Clifford tras 6 T, K=64 | 8 fases Ether, K=256 | Clifford puro, main anterior → integrado | RSS Clifford puro antes → después (KiB) |
| --- | --- | --- | --- | --- |
| 128 | 24.913 → 0.952 | 2.797 → 0.548 | 11.739 → 12.845 | 2240 → 2528 |
| 256 | 90.030 → 1.140 | 8.331 → 1.294 | 33.015 → 32.032 | 2468 → 2788 |
| 512 | 302.258 → 1.568 | 25.811 → 4.326 | 103.418 → 103.326 | 2920 → 3320 |
| 1000 | 1113.585 → 2.104 | 97.791 → 14.692 | 312.017 → 299.720 | 4396 → 5052 |

En Clifford puro, los programas comparados tienen 48·n puertas. A 128 qubits
la mediana integrada es un 9,4 % más lenta; a 1000, un 3,9 % más rápida, con
más RSS. El cambio de autoridad tiene un coste que no queda oculto tras el
resultado de otra etapa. A 1000 qubits la fase Ether usa también más RSS al
final de la ejecución (4024 → 4408 KiB en estas muestras).
Los p95 y todas las muestras están en el JSON, sin extrapolación universal.

Con `factor_pair_shortcuts` desactivado, el producto con enlaces redundantes
agota el presupuesto 256 y se rechaza atómicamente en los cuatro tamaños.
Con la identidad exacta activada termina en los cuatro, manteniendo factores
locales. No se calcula un speedup entre una ejecución rechazada y otra completa.

### 1000 qubits: otros casos completos

| Familia | Puertas locales | Tiempos de ejecución sumados (ms) | Coeficientes Reduced / amplitudes locales | Pico total (MiB) |
| --- | --- | --- | --- | --- |
| partial, profundidad 4 | 1133 | 5.87 | 4 / 31488 | 6.59 |
| mixed, profundidad 16 | 2553 | 73.19 | 64 / 0 | 6.61 |
| ether, profundidad 8 | 2507 | 75.75 | 256 / 0 | 6.66 |
| clifford, profundidad 32 | 96000 | 978.66 | 0 / 0 | 10.29 |
| ghz, profundidad 64 | 1064 | 87.47 | 2 / 0 | 5.93 |
| separable, profundidad 8 | 25000 | 31.16 | 0 / 2000 | 6.77 |

Las capas globales Ether se indican por separado en la definición del caso;
no se cuentan como n puertas locales materializadas. El caso parcial conserva
un componente Reduced de 16 qubits tras CX/CX entre dos grupos de ocho: su
estado vuelve a factorizar matemáticamente, pero la democión general sigue
pendiente. La función del caso es hacer visible también ese límite.

### Observación paralela: 1024 consultas, K=1024

| Trabajadores | Mediana (ms) | p95 (ms) | RSS tras consultas, mediana (KiB) |
| --- | --- | --- | --- |
| 1 | 2235.92 | 2253.66 | 6432 |
| 2 | 1201.58 | 1924.91 | 7032 |
| 4 | 952.15 | 1134.88 | 8232 |
| 8 | 489.62 | 576.61 | 10584 |

Estas siete muestras se intercalaron 1/2/4/8 y 8/4/2/1 tras un calentamiento
por modo; el JSON conserva el hash del probe y el punto de lectura de RSS.
Ocho trabajadores reducen la mediana 4.57 veces con mayor RSS. El valor por
defecto sigue siendo uno; el paralelismo necesita suficientes consultas.

## Persistencia y concurrencia

El recorrido CLI difícil escribe un RVPC2 de 18096375 bytes para 1000 qubits,
65536 coeficientes y 16 T independientes. Restaura, ejecuta T† y CX/CX distantes,
consulta un estabilizador y un byte de memoria, y rechaza la T 17 conservando
los archivos de entrada y destino. El [JSON CLI](measurements/integrated-cli-local.json)
conserva tamaños, revisiones y hash del estado. La matriz nativa conserva los
tiempos de guardar/restaurar por separado.

`integration/parallel_observation.hpp` ofrece consultas Pauli de solo lectura
sobre copias de sesión con payloads inmutables compartidos. Cada trabajador
posee sus diagnósticos Ether y scratch; la base Reduced se inicializa mediante
`call_once`. Se devuelven los resultados juntos tras unir todos los trabajadores.
Se propagan excepciones sin mutar la autoridad. Los límites son 1..8 trabajadores,
4096 consultas y 65536 términos de entrada. No introduce escritores concurrentes.

El perfil mide 1024 estabilizadores de un cluster de 1000 qubits con 10 T y
1024 coeficientes, comparando 1/2/4/8 trabajadores. El llamador elige el número;
el valor predeterminado sigue siendo 1. Las ventajas dependen de CPU y carga.

## Límites que siguen presentes

- 1000 qubits y 65536 coeficientes por componente Reduced por defecto. La
  siguiente T independiente puede requerir 131072 coeficientes y se rechaza.
  Otras T dependientes pueden no expandir y un circuito distinto puede agotar
  el presupuesto mucho antes. T contadas no equivalen a dificultad genérica.
- Los casos separados admiten miles de T porque mantienen factores locales.
  No representan una simulación eficiente de estados arbitrarios de 1000 qubits.
- El crecimiento máximo de una fase necesita scratch de hasta dos veces el
  número previo de coeficientes antes de comprobar el resultado. Los límites
  lógicos no son cuotas estrictas de RSS del proceso.
- Continúan pendientes la partición/democión general de Reduced grandes y
  un plan para evitar la cola de residuos de redondeo al deshacer muchas T.
  No se eliminan coeficientes no nulos por tolerancia para acelerar las inversas.
- Los bloques densos son locales (8 por defecto, máximo configurable 14).
  La separación local se valida por reconstrucción; no se anuncia una
  descomposición general exacta de cualquier estado grande.
- Transporte mixto de núcleo: 64 MiB; contenedor CLI combinado: 32 MiB.
  El control clásico de flujo XI todavía no está implementado.
- La máquina es software sobre CPU clásica. Los resultados no son ventajas
  universales ni equivalen a recursos cuánticos físicos.
