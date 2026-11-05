# Autoridad cuántica integrada de REVERSE-PC

## Qué se ejecuta ahora

XI, Inverse Ether Memory y Ether consultan y transforman **el mismo InfiniteCore**.
Su tabla de direcciones apunta a componentes cuyo `QuantumBlock` contiene una
sola alternativa activa: CompactCore, AdaptiveReduced o Reduced. No existe un
registro no-Clifford paralelo que deba sincronizarse con un registro Clifford.

- Las puertas Clifford conservan el tableau compacto mientras sea suficiente.
- Una T/T† sobre un componente pequeño lo transfiere a AdaptiveReduced.
- Un componente ancho se importa directamente al Reduced canónico desde su
  tableau actual. No se reproduce el programa ni se construye 2^n amplitudes.
- Al conectar componentes se transfieren y concatenan sus estados actuales.
  Los productos pequeños usan AdaptiveReduced; los anchos, el tensor de bases
  estabilizadoras y cosets Pauli de Reduced.
- CX/CZ entre componentes mixtos usa identidades de autoestados comprobadas
  con igualdad algebraica de coeficientes. No hay umbral para borrar residuos.

El motor `reverse_pc_factorized` continúa como herramienta de ensayo; su
AdaptiveReduced también participa ahora dentro de la autoridad usada por XI.
El checkout Mundo X conserva `e2778d962af22f6f1a0794058dd4685fc2573966`.
Las extensiones locales verifican cinco encabezados por SHA-256 y publican
su diff; el código original sigue disponible como referencia independiente.

## Ether y observación

El estado físico es `U_Ether |phi>`. Ether mantiene las capas globales admitidas
y transforma un observable mediante `U† P U`. La consulta resultante se evalúa
sobre los componentes de phi, incluidos los que contienen coeficientes
no-Clifford. Las posiciones no materializadas siguen su fondo analítico.

Una T física bajo Ether se convierte en una fase sobre el Pauli conjugado.
Cuando su soporte ya pertenece a un componente ancho, se aplica directamente
al Reduced. Para otros soportes se usa un cambio de base Clifford, la fase y
su inversa. Ambos caminos se verifican contra un oráculo vectorial independiente.

Reduced conjuga cosets y base para las instrucciones Clifford sin rehacer una
eliminación por puerta. Una [certificación exacta](unchanged-cosets.md) conserva
el mapa de coeficientes cuando la puerta conmuta con todos sus Paulis; el
tableau se transforma siempre. La base de observación se calcula cuando se necesita,
se comparte como caché inmutable y se invalida al cambiar el tableau. La
inicialización es segura para observaciones concurrentes de copias de sesión.
Una caché no es una segunda autoridad del estado.

La [campaña grande](integrated-scale-campaign.md) mide los costes de esas rutas
con circuitos iguales y publica resultados de 128/256/512/1000 qubits. Incluye
una fachada opcional de consultas paralelas de solo lectura, acotada a ocho
trabajadores, con resultados y límites explícitos.

## Probabilidades, bytes y precisión

El determinismo es una propiedad algebraica separada del número redondeado de
la probabilidad. Se comprueba el residual de P|psi> = ±|psi> sin tolerancias.
Una probabilidad representable muy próxima a 0/1 no autoriza un byte clásico.
InverseMemory devuelve un handle para cualquier bit no determinista y nunca
lo mide para satisfacer un read o store.

Las probabilidades de ambas ramas se calculan con las normas de los
proyectores, evitando la cancelación de `1-expectación`. Una prueba conserva
una rama de probabilidad 1e-300 y comprueba su proyección. Si una probabilidad
no cabe en double se rechaza su consulta/proyección explícitamente; no se
sustituye por determinismo. Las sumas de observables son deterministas y
compensadas. Las amplitudes y coeficientes siguen siendo IEEE double, no
aritmética simbólica de precisión ilimitada.

Las mediciones generales usan la probabilidad de Born; 1/2 conserva el muestreo
binario y la secuencia RNG históricos. Las demás comparan palabras del RNG
portable con la expansión binaria del double recibido, incluyendo subnormales,
en un máximo de 17 palabras. Se evita la cuantización de un uniforme de 53 bits;
véanse la [demostración, pruebas y costes](born-sampling.md). Las fases relativas
se conservan; los tableaus fijan una fase global convencional, como el núcleo
Clifford original.

## Transacciones y persistencia

Un programa transforma candidatos privados de núcleo y memoria. Solo se
confirman tras ejecutar todas las órdenes y producir la salida completa.
Una promoción, expansión, medición o lectura fallida descarta también cambios
previos, revisiones, RNG y salida. Las copias comparten estados confirmados;
solo se separan los componentes que se modifican. La contabilidad de payload
incluye los cambios de capacidad causados por esa separación.

- RVPC2 guarda núcleo y memoria como una unidad, con tamaños y checksum.
- MXI3 contiene los componentes mixtos actuales, revisión, RNG y marco Ether.
- Los payloads locales ADL1/RDX1 guardan factores o base+coeficientes actuales,
  no un log necesario para reconstruir el estado.
- MXIM2 añade la revisión de memoria a los datos y enlaces del MXIM1 anterior.
- Se acepta RVPC1/MXI1/MXI2/MXIM1. Una revisión de memoria que no existía en el
  formato antiguo no se inventa al migrar.

En C++, la pareja durable usa `core_snapshot()` y
`memory_persistent_snapshot()`, restauradas conjuntamente con `restore()`.
`memory_snapshot()` conserva deliberadamente el export MXIM1 histórico. El
CLI usa automáticamente la pareja durable y escribe RVPC2.

Los límites pertenecen al dueño receptor: restore no importa presupuestos
más permisivos desde un archivo. Los errores dejan intacto al receptor. No
hay soporte para escritores simultáneos sobre una misma sesión; las lecturas
sobre copias inmutables pueden compartir las bases de observación.

## Ejemplo completo

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
./build/reverse_pc init initial.rvp
./build/reverse_pc run examples/integrated-128.xi initial.rvp active.rvp
./build/reverse_pc run examples/integrated-128-continue.xi active.rvp continued.rvp
ctest --test-dir build --output-on-failure
```

Los programas ejecutan Clifford y T/T† sobre 128 qubits, atraviesan Ether,
consultan memoria enlazada, guardan, restauran y continúan. La prueba CLI
repite ese recorrido con 128/256/512/1000 qubits. Su modo `--hard` guarda y
restaura además un grafo distribuido de 1000 qubits con 16 T independientes y
65536 coeficientes, comprueba una continuación y rechaza la T 17 sin sobrescribir
los archivos. La referencia RVPC1 se genera con el código y las extensiones
reales de main anterior, no con un supuesto formato inventado.

## Límites verificables

| Recurso | Contrato actual |
| --- | --- |
| Registros ensayados | 128, 256, 512 y 1000 qubits con puertas reales |
| Componente Reduced | 1..1000 qubits; 65536 coeficientes por defecto |
| Bloque denso integrado | 8 por defecto; configurable 0..14; nunca global |
| Active qubits de InfiniteCore | 4096 por defecto; dominio lógico por direcciones decimales |
| Presupuesto de payload | 64 MiB por defecto, distinto de RSS físico |
| Transporte de núcleo mixto | 64 MiB; contenedor RVPC2 combinado limitado a 32 MiB |
| No-Clifford difíciles | El coste depende también del número de cosets independientes, no solo de qubits |

Un grafo de 1000 qubits con 16 T no equivale a un circuito genérico de 1000
qubits con T arbitrarias. La expansión puede crecer exponencialmente y se
rechaza al superar el presupuesto. Los cambios de representación y sus
copias transaccionales también tienen costes medibles.

Pendientes: partición/democión general exacta de componentes Reduced grandes,
transferencias que crucen su límite de 1000 qubits, control clásico de flujo
XI y formatos que excedan el transporte actual. No se recortan amplitudes para
aparentar que esos límites han desaparecido.
