# Muestreo Born sin rejilla fija de 53 bits

## Corrección

La integración calculaba por separado las probabilidades de ambos proyectores,
conservando ramas tan pequeñas como 1e-300. El muestreador convertía una palabra
RNG en un uniforme de 53 bits. Para un p positivo menor que 2^-53, ese método
aceptaba únicamente el punto cero de su rejilla: su probabilidad ideal de
aceptación era 2^-53, no p. La corrección elimina esa cuantización.

`integration/born_sampling.hpp` compara palabras de 64 bits con la expansión
binaria de la probabilidad double. En cada paso:

1. Multiplica p por 2^64 y toma su parte entera k.
2. Si la palabra aleatoria es menor que k, acepta; si es mayor, rechaza.
3. Si coincide, continúa con la parte fraccionaria, o rechaza si esta es cero.

La comparación resuelve 64 bits por paso. Multiplicar por una potencia de dos
es exacto en este rango. Si el resultado tiene más de 53 bits significativos
enteros, ya es un entero representable; en los demás casos su parte entera y
su resta son representables exactamente. No se introducen nuevos redondeos en
el umbral. Todo double IEEE-754 de [0,1] tiene una expansión binaria finita de
como máximo 1074 bits: se necesitan como máximo 17 palabras RNG.

Condicionado a palabras uniformes de 64 bits, la probabilidad de aceptación
es el p double recibido. Esto no cambia la precisión de las amplitudes ni
convierte el PRNG reproducible en una fuente de aleatoriedad física.

Se toma directamente la probabilidad del proyector más raro: no se calcula
`1-p` para reconstruir una rama que podría perderse por cancelación. Cuando
ambas probabilidades son exactamente 1/2 se mantiene el bit bajo de una sola
palabra, conservando el historial RNG Clifford y su coste de acceso.

Los snapshots guardan el mismo estado RNG y no cambian de formato. Prefijos
que antes sufrían cuantización pueden consumir más palabras o producir otro
resultado con la corrección. La restauración y continuación dentro de la
versión corregida son reproducibles; los 175 contratos Clifford históricos
siguen siendo idénticos byte a byte.

## Pruebas

- 175780 comparaciones de umbrales diádicos exhaustivos y comprobaciones de
  frecuencias sobre sus prefijos finitos, con probabilidades k/2^m, m=1..8.
- 3221 casos de potencias de dos, hasta el subnormal mínimo 2^-1074, con palabras
  controladas a ambos lados y en la igualdad del umbral.
- Un prefijo controlado `0, UINT64_MAX` con p=1e-300 demuestra el antiguo sesgo:
  el uniforme de 53 bits acepta; la comparación completa rechaza.
- La rama mínima y su complemento cercano a uno conservan la decisión usando
  las 17 palabras necesarias. No se intentó observar por Monte Carlo un evento
  de probabilidad 1e-300.
- 10000 decisiones 1/2 conservan resultados y estado final del generador.
- Probabilidades inválidas fallan antes de consumir RNG; 0 y 1 no lo consumen.
- Las pruebas integradas conservan Born, rollback, persistencia, restauración,
  fondos Bell/Ether y el recorrido CLI de 128/256/512/1000 qubits con el caso
  adversarial de 65536 coeficientes.

## Coste medido

El [JSON local](measurements/born-sampling-local.json) conserva siete muestras
alternadas de un millón de decisiones por modo, GCC 13.3, C++17, -O3, el mismo
mt19937_64 y semilla, hashes de fuentes/ejecutable y RSS. Aísla el muestreador;
no mide una proyección cuántica completa ni atribuye su coste al RNG real de
InfiniteCore. Se compara con la función anterior de 53 bits.

| P(1), P(0) | Anterior, mediana ns | Corregido, mediana ns |
| --- | --- | --- |
| 0.5, 0.5 | 3.66 | 4.07 |
| 0.14645, 0.85355 | 4.06 | 15.95 |
| 1e-300, 1 | 3.74 | 13.24 |
| 1, 1e-300 | 3.24 | 10.89 |

RSS/HWM del proceso completo: 1796 KiB. Cada modo consumió un millón de
palabras en estas muestras ordinarias; el peor caso de 17 palabras se verifica
con las entradas controladas de la prueba. Los p95 están en el JSON.

La corrección general tiene un coste de CPU. No se modifica el presupuesto de
componentes ni se reserva memoria dinámica dentro del muestreador. Los casos
raros del microbenchmark no demuestran frecuencias empíricas de 1e-300; su
exactitud se verifica mediante umbrales controlados y la construcción binaria.

```sh
./build/reverse_pc_born_sampling
./build/reverse_pc_born_sampling --benchmark
ctest --test-dir build -R 'born_sampling|integrated_correctness|integrated_cli' --output-on-failure
```
