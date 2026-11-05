# REVERSE-PC

**Compatibilidad ARM64 en XI:** módulo del ejecutor de REVERSE con memoria por
imagen/reglas y deltas XI/RXE, modo de sistema EL1/EL0, MMU, excepciones,
temporizadores y consola. Conserva el motor cuántico original y se abre en
**XI → ARM64**. Android y Wild Rift aún no arrancan.
[Capacidades, pruebas y límites](docs/MINIMAL_STATE_GUEST.md).

**REVERSE 2.5.1:** BBY Q E025 y W4u 0.1 integrados, memoria nativa ampliada y correcciones de teclado conservadas. [Fuentes, compilación y procedencia](docs/RELEASE_2_5_1.md).

**REVERSE 2.4.2:** escritorio PC horizontal con teclado integrado, borrador
continuo en BBY Q y espacio de escritura ajustado al teclado Android.
[Teclado, actualización y validación](docs/REVERSE_KEYBOARD_2_4_2.md).
[Escritorio y ventanas](docs/REVERSE_DESKTOP_2_4_1.md).

**W4u 0.1 — work for u:** BBY Q conversa y W4u coordina tres roles XI para
operar la interfaz y ejecutar programas RXE internos.
[Uso, arquitectura y pruebas](docs/W4U_BBYQ.md).

## BBY Q controla Workbench 2.2

BBY Q E022 puede abrir herramientas, enfocar, minimizar, maximizar, restaurar y
cerrar ventanas, ordenarlas, mostrar el escritorio y cambiar de espacio. Las
órdenes ES/EN se interpretan en XI; el escritorio confirma el resultado antes de
mostrar «Acción completada». Se conservan el conocimiento anterior y los estados
privados por ejecutable. [Uso, pruebas y límites](docs/BBYQ_INTERFACE_CONTROL.md).

## Ejecutables propios RXE

El **BBY-Q.rxe original** contiene BBY Q E016, su programa compilado y su memoria entrenada.
REVERSE Desktop RXE 1.2 lo abre con conversación ES/EN, contexto persistente y
exportación de memoria. [Contrato, procedencia y límites](docs/BBYQ_RXE_APP.md).

Los programas de REVERSE pueden distribuirse como **`.rxe`**: imágenes binarias
con cabecera, ABI versionada, constantes, opcodes, operandos tipados y saltos
resueltos. `reverse_pc compile fuente.xi programa.rxe` genera el ejecutable y
`reverse_pc run programa.rxe entrada.rvp salida.rvp` lo carga en el mismo
QuantumRuntime. El escritorio permite crear, abrir, agregar, exportar y anclar
esos programas. [Formato y límites de RXE](docs/RXE_EXECUTABLES.md).

RXE v1 usa la ABI XI Machine v1 y un cargador que la entrega al intérprete
transaccional original. No cambia la autoridad de estado ni el snapshot RVPC2.

Computadora de software para computación cuántica simulada: espacio lógico por
reglas, componentes compactos y una autoridad de estado compartida por XI,
Ether e Inverse Ether Memory. Ejecuta en CPU clásica.

## Arquitectura integrada

InfiniteCore posee una tabla de direcciones y componentes. Cada componente
contiene exactamente una representación activa: CompactCore para Clifford,
AdaptiveReduced para bloques pequeños o Reduced para estados no-Clifford
anchos. T/T† promueven el estado **actual**, sin reproducir el historial.
Los componentes conectados se combinan mediante transferencias y tensores.

Ether mantiene el marco global y propaga los observables hacia atrás sobre el
mismo estado. InverseMemory conserva reglas, bytes y enlaces explícitos;
un byte no determinista devuelve un handle y no se mide automáticamente.
XI ejecuta todas esas operaciones con rollback conjunto de núcleo, memoria,
RNG, revisiones y salida.

[Contrato completo, precisión y capacidades pendientes](docs/unified-authority.md).
El [muestreo Born binario](docs/born-sampling.md) conserva las probabilidades
double representables, incluidas subnormales, y la secuencia histórica de 1/2.

## Compilar y ejecutar

```sh
bash scripts/setup-ether.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/reverse_pc init initial.rvp
./build/reverse_pc run examples/integrated-128.xi initial.rvp active.rvp
./build/reverse_pc run examples/integrated-128-continue.xi active.rvp continued.rvp
```

El ejemplo ejecuta entrelazamiento de 128 qubits, T/T†, Ether, memoria enlazada,
observación, guardado y continuación. `qstats` informa los tipos de componente,
coeficientes, amplitudes locales, payload estimado y norma.

RVPC2 guarda el estado actual y conserva también la revisión de memoria.
Se aceptan archivos RVPC1 anteriores. La prueba genera un RVPC1 con el código
y extensiones preservados de main anterior para comprobar la migración real.
En C++, la pareja durable usa `core_snapshot()` y
`memory_persistent_snapshot()`, con restauración conjunta.

## Máquina programable XI — perfil de control clásico

La nueva capa de máquina (`#!reverse-pc-machine-v1`) amplía el **mismo** intérprete
transaccional con 16 registros enteros de 64 bits, aritmética comprobada,
comparaciones, saltos, ciclos y acceso byte a byte a Inverse Ether Memory.
Se admiten operandos dinámicos `$r0` en las direcciones de puertas cuánticas
y términos como `Z:$r0`: un ciclo puede construir realmente un circuito
de 128 qubits sobre InfiniteCore sin enumerarlo línea por línea.

[Contrato, instrucciones, seguridad, limitaciones y reproducción](docs/xi-programmable-machine.md).

```sh
./build/reverse_pc init initial.rvp
./build/reverse_pc run examples/machine-ghz-128.xi initial.rvp machine-state.rvp
./build/reverse_pc run examples/machine-continue.xi machine-state.rvp continued.rvp
```

El perfil es **opt-in**: los programas XI anteriores no cambian y conservan su
gramática, resultados y RNG originales. Los registros son temporales por programa;
los bytes de InverseMemory y el estado cuántico sí persisten en RVPC2. Un error
en cualquier punto, incluido el presupuesto de pasos, descarta toda la transacción
y no publica una salida parcial. Es una primera CPU virtual/ISA de software,
no hardware físico ni una computadora personal con sistema operativo.

## Pruebas y escala

Se conservan las pruebas históricas, la auditoría recuperada de PR #5,
oráculos densos independientes, reversibilidad, fuzzing de snapshots y
ASan/UBSan/LeakSanitizer en CI. La comparación Clifford histórica conserva
salidas, errores, snapshots y RNG idénticos en los 175 casos anteriores.

La ruta integrada se prueba con operaciones reales en 128, 256, 512 y 1000
qubits. La prueba CLI recorre esos tamaños y su modo `--hard` guarda/restaura
un grafo distribuido de 1000 qubits con 16 T independientes y 65536
coeficientes; la T 17 se rechaza sin sobrescribir estado ni archivos.

```sh
./build/reverse_pc_integrated_correctness
python3 tests/integrated_cli.py ./build/reverse_pc output ./build/reverse_pc_previous_cli --hard
```

[Prueba de persistencia local](docs/measurements/integrated-cli-local.json).
La [campaña inicial medida](docs/integrated-scale-campaign.md) contiene 60 casos grandes,
hasta 96000 puertas Clifford, grafos con entropía 500 entre mitades antes de T,
presupuestos de 16 a 65536 coeficientes y comparaciones de mediana/p95.
A 1000 qubits, 96 Clifford tras seis T pasan de 1114 a 2,10 ms al diferir la
canonicalización; ocho fases bajo Ether pasan de 97,8 a 14,7 ms. Son tiempos
de esas etapas. Clifford puro también conserva su comparación, incluidos
sobrecostes de memoria y una regresión temporal a 128 qubits.

```sh
python3 scripts/integrated-matrix.py ./build/reverse_pc_integrated_large \
  --legacy ./build/reverse_pc_legacy_large --parallel ./build/reverse_pc_parallel_observation \
  --full --repeats 7 --checkpoint evidence/checkpoint.json > evidence/matrix.json
```

La [certificación posterior de cosets](docs/unchanged-cosets.md) evita reconstruir
coeficientes que una Clifford deja idénticos, conservando la transformación del
tableau. A 1000 qubits y 65536 coeficientes, 96 CZ pasan de 1046 a 290 ms y de
6291936 a 384 asignaciones. Las familias afectadas conservan sus resultados y
regresiones en el informe.

En una pareja completa con configuración idéntica, la inversión adversarial
pasa de 29,44 a 2,95 s; el pico total sube de 197 a 213 MiB al incluir copias,
restauración y comprobaciones. El payload lógico de unos 18 MB no equivale a
ese pico físico. Los resultados no representan un circuito genérico de 1000
qubits con un número arbitrario de T.

## Límites y dependencia

- Mundo X permanece fijado a `e2778d962af22f6f1a0794058dd4685fc2573966`.
  Las extensiones de compilación verifican SHA-256 de los encabezados; el
  checkout original no se modifica. CI publica el diff utilizado.
- Reduced admite componentes de hasta 1000 qubits; el presupuesto
  predeterminado es de 65536 coeficientes, con rechazo atómico al excederlo.
- Los vectores densos integrados son **locales**, de 8 qubits por defecto y
  hasta 14 configurables. No se crea un vector de 2^1000 amplitudes.
- Norma, amplitudes y observables usan precisión flotante explícita; no se
  podan contribuciones no nulas para aparentar una mejora.
- Payload lógico, memoria residente y dominio de direcciones son métricas
  distintas. Las ventajas dependen del circuito y de su estructura.
- Faltan partición/democión general de componentes Reduced grandes y control
  clásico de flujo XI. No se afirma rendimiento universal ni hardware cuántico.

## Informes anteriores y herramientas conservadas

- [Transferencias desde el estado actual](docs/current-state-transfer.md).
- [Factorización AdaptiveReduced y benchmark independiente](docs/factorization.md).
- [Separación de bloques: resultados y sobrecostes GHZ](docs/dynamic-refactorization.md).
- [Historial compacto y fallback de productos](docs/history-and-transfer.md).
- [Transacciones XI con revisiones conservadas](docs/xi-program-candidates.md).
- [Estado actualizado y siguiente iteración](docs/NEXT_ITERATION_2026-10-25.md).

Los informes anteriores conservan sus condiciones y revisiones originales.
Sus números particulares no se extrapolan a todos los programas.

## REVERSE OS — Android desktop v0.2

La interfaz de REVERSE-PC inicia con la pantalla negra "Welcome, Mr Reverse" y abre un escritorio con seis apps y dock: XI Studio, Terminal, Files, Ether Core, Chromium mediante Android WebView y Settings. Los programas XI siguen ejecutándose mediante el mismo QuantumRuntime nativo con persistencia RVPC2. Android continúa siendo el sistema operativo anfitrión; este producto no es aún un kernel arrancable. [Documentación de REVERSE OS](reverse-os-android/README.md).

## REVERSE OS v0.3 — escritorio horizontal y orbital animado

La versión Android adopta una interfaz de escritorio convencional (similar a Windows/macOS) con Inicio, iconos, búsqueda, barra inferior y ventanas flotantes. El fondo violeta orbital sí se anima mediante el motor gráfico GLES3 de Space Glitch V2, adaptado para REVERSE OS. XI Studio y Terminal ejecutan los programas del mismo QuantumRuntime/InverseMemory y conservan el estado RVPC2. Es un entorno Android anfitrión, no un kernel arrancable ni una ejecución de Chromium bajo XI. [Detalles de compilación y pruebas](reverse-os-android/README.md).


## REVERSE Desktop 1.0

El escritorio Android incorpora varias ventanas no modales, barra de tareas,
restauración de sesión, carpetas y documentos, borradores, copiar/mover/renombrar,
papelera, imágenes, aplicaciones instaladas y calculadora ejecutada en XI.
La edición `com.reversepc.desktop` se instala junto a versiones anteriores.
El runtime, el formato RVPC2 y la dependencia fijada de Mundo X se conservan.
[Uso, compilación y límites de REVERSE Desktop](reverse-os-android/README.md).
