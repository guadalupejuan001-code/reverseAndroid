# Candidatos privados de programas XI — 25 octubre 2026

## Propiedad conservada

Un `QuantumRuntime::execute` ya transforma un candidato privado del núcleo y
otro de InverseMemory. La autoridad pública confirma ambos solo tras completar
todo el programa y la salida. Dentro de ese candidato, repetir copias del
mapa de ubicaciones por instrucción no aporta aislamiento adicional.

La extensión local, verificada por SHA-256 contra los encabezados fijados,
añade una marca privada que solo QuantumRuntime puede activar. `apply()` puede
modificar metadatos sin copiar cuando la marca está activa **y** `state_.unique()`.
La primera escritura todavía separa el estado compartido. Los componentes
compartidos conservan su copy-on-write original. Cada apply conserva la
validación y su incremento de revisión. Antes de publicar el candidato se
retira la marca. Ninguna marca se guarda en el snapshot.

Una excepción puede dejar incompleto el candidato privado, que se descarta;
no se publica al dueño, no cambia su RNG y no afecta a sus bifurcaciones.
Fuera de execute, InfiniteCore conserva su transacción atómica por operación.
No se reescribe el programa ni se añaden begin/end implícitos.

## Verificación contra el código original

`tests/runtime_contract.cpp` se compila dos veces: el target de referencia da
precedencia a los includes del checkout **sin modificar**; el otro emplea la
extensión. `tests/compare_runtime.py` exige igualdad byte por byte del transcript
completo: salidas, errores, instantáneas de núcleo/memoria y RNG. Los 175 casos
producen 4.534.143 bytes idénticos; [SHA-256 del transcript](measurements/xi-contract-local.json).
Incluye 160
programas aleatorios y casos de medición, Ether global, fondo Bell,
bifurcaciones, restauración, errores de sintaxis, presupuestos de memoria,
4.096 activos y desbordamiento del contador de revisión. Las revisiones siguen
siendo 129 para 128 instrucciones tras la inicialización, y 2 para un batch
explícito. Las operaciones públicas directas siguen rechazándose atómicamente.

Once suites Release y nueve suites de corrección instrumentadas ASan/UBSan.
CI conserva LeakSanitizer; localmente su acceso a /proc no está disponible.
La huella del script de extensión forma parte de las opciones de compilación:
añadir un encabezado que sombrea un include anterior invalida también los
objetos incrementales, aunque su viejo archivo .d apuntase al upstream.

## Comparación con condiciones idénticas

21 procesos por caso, calentamiento y orden alternado, C++17 Release -O3.
Se cronometran las 128 instrucciones individuales; la inicialización queda
fuera en ambos motores. La revisión forma parte de la comprobación, no solo
la probabilidad final. RSS corresponde a VmRSS/VmHWM del proceso después de exec.

| Activos | Original ms mediana / p95 | Extensión ms mediana / p95 | Pico RSS KiB original / extensión | Revisión original / extensión |
| --- | ---: | ---: | ---: | ---: |
| 1 | 0.108325 / 0.370078 | 0.081409 / 0.224023 | 1992 / 1992 | 129 / 129 |
| 64 | 0.622828 / 1.651134 | 0.085772 / 0.244801 | 2036 / 2024 | 129 / 129 |
| 256 | 1.910789 / 3.662819 | 0.095830 / 0.286083 | 2148 / 2124 | 129 / 129 |
| 1024 | 8.639202 / 13.390056 | 0.217126 / 0.433988 | 2616 / 2544 | 129 / 129 |
| 4096 | 35.279752 / 58.727491 | 0.692861 / 1.684518 | 4496 / 4132 | 129 / 129 |

[Mediciones agregadas](measurements/xi-runtime-local.json); CI publica las
muestras y el transcript resumido con SHA-256. No son comparaciones con un
batch que modifica la revisión ni con hardware cuántico.

## Límites

La mejora se aplica a puertas dentro de una llamada execute. Un programa de
una sola puerta todavía paga su separación inicial; procesos nuevos y
transacciones independientes conservan sus costes. Otras operaciones que
crean un candidato propio (p. ej. capas globales o mediciones) mantienen su
implementación anterior. `background` puede sustituir el candidato por un
núcleo nuevo, desactivando la optimización para el resto de esa ejecución sin
cambiar el resultado. No se altera RVPC1 ni se promete compatibilidad ABI de
objetos compilados con mezclas de encabezados originales y extendidos.
