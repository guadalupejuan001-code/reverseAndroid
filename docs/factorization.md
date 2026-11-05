# Adaptive Reduced — factorización con retroceso canónico

## Qué se implementó

`src/adaptive_reduced.hpp` es una capa de ejecución nueva de REVERSE-PC,
sobre el backend Reduced original de Mundo X, sin cambiarlo.

- Empieza con un producto tensorial de qubits independientes: un bloque de
  dos amplitudes por qubit.
- Las puertas H/X/Z/S/T/T† solo modifican el bloque afectado.
- Las puertas CX/CZ entre bloques verifican si el control está definido en
  Z o si el objetivo es un autoestado adecuado (X para CX; Z para CZ). En
  esos casos aplican una identidad algebraica que no genera entrelazamiento.
- En los demás casos se fusionan los bloques y se aplica la puerta completa
  sobre el vector resultante.
- Si la fusión superaría `max_dense_block` (predeterminado 14 qubits),
  se importa directamente el producto actual cuando todos los bloques son
  de un qubit; para bloques entrelazados o si la importación no cabe, se
  conserva el replay canónico del historial compactado. Una falla no confirma
  el estado. Véase [contrato y extensión local de compatibilidad](history-and-transfer.md).
- No hay truncamiento ni poda de contribuciones. Una simplificación
  requiere residual vectorial exactamente cero en la representación de
  punto flotante. Si hay diferencias, incluso diminutas, se fusionan
  bloques en vez de descartar amplitudes. La precisión aritmética sigue
  estando limitada por el formato double.

La nueva ruta ejecutable es `reverse_pc_factorized`, no el ejecutable
anterior `reverse_pc`, que conserva sus semánticas XI y estados RVPC1.

## Compilar y ejecutar

```sh
bash scripts/setup-ether.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/reverse_pc_factorized --self-test
./build/reverse_pc_factorized --benchmark
./build/reverse_pc_factorized --run 4 examples/factorized-small.qxi
```

Admite instrucciones de texto `h`, `x`, `z`, `s`, `t`, `tdg`,
`cx` y `cz` con índices desde cero y una instrucción por línea.
No es todavía un intérprete XI completo y no persiste instantáneas .rvp.

## Reproducción de los números

`--benchmark` ejecuta exactamente H en todos los n qubits, luego la
cadena de CNOT vecinos y luego T en todos ellos, para n = 8, 10, 12,
14 y 16. Ejecuta cinco veces ambas implementaciones sobre el mismo
servidor, informa la mediana de los tiempos y comprueba el estado final
contra la representación canónica Reduced hasta una fase global.

El informe también ejecuta un caso separable de 1.000 qubits únicamente
con el motor factorizado, sin extrapolar una comparación con Reduced que no
se ha ejecutado.

Los tiempos excluyen la reconstrucción del vector denso de verificación.
Los bytes informados son *estimaciones de payload*, no RAM del proceso.
Para todos esos circuitos el estado es separable; 2^n componentes no
son necesarios para representar ese estado.

`--self-test` verifica 260 circuitos aleatorios con oráculo denso
independiente, estados entrelazados Bell, fallback a Reduced, un circuito
factorizable de 1000 qubits, observables, y validación de índices.

La ejecución de CI publica `factorized-tests.json`,
`factorized-benchmark.json` y `factorized-program.json` como evidencias.

## Límites

Esta técnica ahorra recursos cuando los estados **tienen estructura de
producto o bloques pequeños**. Para entrelazamiento que atraviesa el límite
de bloque, vuelve a Reduced, conservando su complejidad y sus límites.
No hace eficientes los circuitos cuánticos arbitrarios, no ejecuta
hardware cuántico y no demuestra ventaja frente a otros simuladores.
El replay de bloques entrelazados puede seguir dependiendo de la longitud del
historial; el nuevo caso producto evita ese coste. Las mediciones y los
límites están en [historial y transferencia](history-and-transfer.md).
