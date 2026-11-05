# Clifford: conservar coeficientes que no cambian

## Cambio funcional

Una instrucción Clifford sobre Reduced conjugaba todos los Paulis del mapa y
creaba un nodo nuevo por coeficiente, incluso cuando esos operadores conmutaban
con la puerta. La nueva ruta comprueba primero esa condición. Si todos quedan
idénticos, conserva el mapa y transforma el tableau; si alguno cambia, usa el
método anterior con un candidato completo.

Esto mantiene una sola autoridad y no descarta coeficientes pequeños. Tampoco
elimina la puerta física: el tableau siempre recibe la instrucción, se invalida
la base derivada de observación y se conservan las revisiones del propietario.
`clifford_current(..., false)` conserva el camino de referencia para las pruebas.

Para una clave Pauli `X^x Z^z`, la exploración busca estas condiciones:

| Puerta | Puede cambiar la clave o su fase si… |
| --- | --- |
| H(a) | x[a] o z[a] |
| S(a), Z(a) | x[a] |
| X(a) | z[a] |
| CX(a,b) | x[a] o z[b] |
| CZ(a,b) | x[a] o x[b] |

Se incluyen los signos de Y bajo H y las fases de S/CZ. La exploración termina
al encontrar una clave afectada. Un resultado negativo certifica por inspección
algebraica que todos los coeficientes pueden conservarse. No se usa tolerancia,
un resumen potencialmente obsoleto ni una caché que necesite invalidación.

El chequeo cuesta O(K) en el peor caso. La reconstrucción evitada también era
O(K), pero incluía hashing de claves multiword, asignación/copia y liberación de
K nodos. Las instrucciones que afectan a los coeficientes siguen realizando
ese trabajo y añaden el coste de buscar el primer testigo. La ventaja depende
del circuito; se mide también esa familia sin ahorro.

## Corrección y transacciones

El tableau candidato y el nuevo mapa, cuando es necesario, se preparan antes
del commit. Una asignación fallida conserva el estado anterior. Las pruebas
comparan 3912 snapshots completos con la referencia, incluyendo operaciones
no-Clifford intercaladas, fases y 128/256/512/1000 qubits. Se inyecta `bad_alloc`
en cada punto de asignación de puertas afectadas y no afectadas: 67 fallos
conservan el snapshot. Se verifican además operandos y operaciones inválidos.

Las suites densas históricas y la matriz integrada completa se ejecutan con la
ruta nueva. La optimización no cambia formatos, norma, RNG ni presupuestos. La
fase no-Clifford siguiente sigue expandiendo y canonicalizando el estado actual.

## Comparación reproducible

```sh
./build/reverse_pc_clifford_cosets
python3 scripts/compare-clifford-cosets.py ./build/reverse_pc_clifford_cosets \
  --repeats 7 --checkpoint evidence/cosets-checkpoint.json > evidence/cosets.json
```

Los 24 casos combinan cuatro tamaños, 8/16 T independientes y tres familias de
96 puertas después del mismo grafo distribuido:

- `commuting`: CZ sobre el grafo; los cosets Z conmutan y el tableau cambia.
- `mixed`: H en 96 posiciones, algunas dentro del soporte no-Clifford.
- `affected`: H en q0 repetida; los coeficientes cambian en cada paso.

Cada muestra es un proceso nuevo. Se calientan ambos modos y se alterna su orden.
Los tiempos/RSS tienen los contadores de asignación desactivados; dos procesos
adicionales cuentan asignaciones y bytes acumulados. El probe usa el mismo
wrapper de `operator new` en ambos modos, además de para inyectar fallos. No es
un tiempo completo de XI: el COW transaccional del componente sigue teniendo
su propio coste, cubierto por la matriz integrada.

RSS se lee después de la fase Clifford y antes de validar; incluye la preparación
del grafo y sus T. Los bytes de asignación son **tráfico acumulado**, no memoria
residente. El payload lógico y K se informan por separado. Norma, estabilizadores
Pauli transformados y digest del snapshot se comprueban en cada proceso.

Medición local: GCC 13.3, -O3, siete muestras alternadas por modo.
[Datos crudos, p95 y hashes](measurements/clifford-cosets-local.json).
El error máximo combinado de norma/Paulis fue 2e-15.

### 65536 coeficientes: mediana antes → después (ms)

| Qubits | 96 CZ que conmutan | H en soporte mixto | H que afecta en cada paso |
| --- | --- | --- | --- |
| 128 | 1154.78 → 321.64 | 1389.56 → 522.57 | 1679.38 → 2058.91 |
| 256 | 1406.18 → 375.35 | 1406.70 → 645.39 | 1962.28 → 1965.00 |
| 512 | 1149.03 → 279.04 | 1220.75 → 501.25 | 1822.97 → 1731.12 |
| 1000 | 1045.71 → 289.79 | 1133.47 → 586.72 | 1772.61 → 1778.71 |

La familia afectada no ahorra asignaciones. Se conservan sus regresiones,
incluida la diferencia de medianas a 128 qubits; sus muestras muestran una
dispersión amplia (p95 de varios segundos). Son resultados descriptivos, no
una garantía de velocidad para todas las instrucciones ni una atribución
causal de cada variación de tiempo a la exploración.

### Asignaciones y memoria a 1000 qubits, 65536 coeficientes

| Recurso para 96 CZ | Referencia | Optimizado |
| --- | --- | --- |
| Asignaciones contadas | 6291936 | 384 |
| Bytes acumulados asignados | 1888306176 | 24675072 |
| RSS posterior, mediana (KiB) | 42604 | 32124 |
| Pico RSS, mediana (KiB) | 42632 | 32264 |
| Payload lógico (bytes) | 18082792 | 18082792 |

El payload y K permanecen iguales. La reducción de tráfico de asignación
no equivale a esa cantidad de RAM liberada: el RSS posterior baja unos 10 MiB
en esta familia. Con 256 coeficientes la reducción de asignaciones también
existe, pero la memoria de los tableaus y del proceso pesa más; el JSON
conserva esos casos.


## Pareja completa mediante XI integrado

El target `reverse_pc_coset_reference_large` compila la misma fuente de la
prueba integrada con `REVERSE_PC_COSET_REFERENCE=1`: únicamente desactiva esta
certificación dentro de QuantumBlock. Ambos ejecutables usan Release y los
mismos argumentos `1000 adversarial 17 65536 1`.

El [JSON de la pareja local](measurements/coset-integrated-local.json) conserva
los hashes de ambos ejecutables y todos sus campos. Son ejecuciones únicas,
complementarias a las siete muestras de cada etapa, sin p95 inferido.

| Métrica completa | Referencia | Optimizado |
| --- | --- | --- |
| Inversa (s) | 29.436 | 2.950 |
| Pico RSS del proceso (MiB) | 197.09 | 213.17 |
| Error máximo de observables comprobados | 1.11e-16 | 1.11e-16 |
| Snapshot de núcleo (bytes) | 18096314 | 18096314 |

Las dos rutas admiten 16 T, mantienen 65536 coeficientes y rechazan la T 17
sin corrupción. Verifican restauración, memoria, revisiones, RNG y la inversa.
El pico global **aumenta** en esta pareja aunque el caso de 96 CZ reduzca RSS:
los temporales, las copias y el asignador forman parte del coste real. La prueba
CLI difícil también continúa guardando/restaurando un RVPC2 real de unos 18 MB.

```sh
./build/reverse_pc_coset_reference_large 1000 adversarial 17 65536 1
./build/reverse_pc_integrated_large 1000 adversarial 17 65536 1
```

## Límites y próxima iteración

El ahorro no cambia el número de coeficientes ni permite una T independiente
adicional sobre el presupuesto. El grafo de 1000 qubits con 16 T sigue teniendo
65536 coeficientes y rechaza la siguiente expansión. Los residuos no nulos de
redondeo permanecen durante las inversas.

Se conserva el coste de copiar el tableau candidato y de construir una caché de
observación nueva por instrucción. Esas asignaciones restantes ofrecen un punto
concreto de perfilado. Evitarlas exige mantener la garantía de excepción y las
lecturas concurrentes; no se supone que sean gratuitas.
