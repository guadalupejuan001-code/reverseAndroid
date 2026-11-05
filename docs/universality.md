# REVERSE-PC: prueba de universalidad computacional

## La pregunta

¿El modelo compacto existente puede ejecutar **un conjunto universal de
operaciones**, y qué costes introduce el número de puertas no-Clifford?

La respuesta tiene varias capas que no deben confundirse:

1. **Universalidad del conjunto de puertas.** H, T y CNOT permiten aproximar
   cualquier operación unitaria con precisión arbitraria. Es un resultado
   matemático conocido, no un descubrimiento nuevo de REVERSE-PC.
2. **Implementación de la construcción.** El módulo C++ demuestra la puerta
   Toffoli exacta usando 7 T/T†, CNOT y H del backend original de Mundo X.
   La lógica Toffoli implementa NAND con un bit auxiliar inicializado en 1.
   También compara un estado coherente (con fases relativas) contra un
   vector analítico calculado independientemente.
3. **Escalabilidad.** El núcleo reducido puede mantener 1.000 qubits lógicos
   para algunos circuitos estructurados. Esto no demuestra que todo
   circuito cuántico universal de 1.000 qubits tenga un coste pequeño.
4. **Integración con REVERSE-PC.** XI ya ejecuta Clifford+T bajo una sola
   autoridad InfiniteCore; véase el [contrato actual](unified-authority.md).
   Este ejecutable conserva la prueba independiente del Reduced canónico.
   Su campo `witness_uses_integrated_xi=false` describe el alcance de esa
   prueba, no una capacidad ausente de XI. El control clásico de flujo sigue
   pendiente y no se afirma eficiencia para circuitos arbitrarios.

## Construcción Toffoli exacta

Para bits 0/1:

    4abc = a+b+c - (a XOR b) - (a XOR c) - (b XOR c)
                      + (a XOR b XOR c)

El factor exp(iπ/4 × expresión) es -1 solo en 111: CCZ. Se
implementa mediante fases T/T† sobre paridades temporales creadas
y deshechas con CNOT, y dos puertas H en el objetivo transforman
CCZ en CCX/Toffoli. Se comprueban 8 entradas de base, las 4
entradas de NAND y amplitudes/fases de superposiciones. El
oráculo se calcula mediante la transformación ideal, **no**
mediante reejecutar el historial de puertas del backend.

## Prueba de 1.000 qubits

Se ejecuta H/T en q999 y CX(q999,q0). Se comprueban:
- Eje X/Y inicial q999 = 1/√2 tras H/T.
- Correlación remota ZZ(q999,q0) = +1.
- Norma ≈1, presupuesto de componentes y número de puertas T.
- Tableau de 257.000 bytes. RAM total y coste del simulador
  se reportan por separado.

## Perfil de dificultad

Se repite la misma familia de circuitos al pasar de 2 a 10 qubits,
incorporando H, conexiones CNOT y T en cada qubit. Se reportan
componentes, bytes lógicos y tiempos; estos datos son evidencia
de **esa familia de circuitos** y no una ley general.

## Uso nativo (sin Python)

Se usa el checkout fijado de Mundo X con extensiones locales verificadas.
CMake verifica la revisión y los encabezados originales; el diff compilado
se publica en CI. También se conservan referencias sin esas extensiones.

    bash scripts/setup-ether.sh
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    cmake --build build --target reverse_pc_universality
    ./build/reverse_pc_universality --self-test
    ./build/reverse_pc_universality --profile
    ctest --test-dir build -R '^reverse_pc_universality_' --output-on-failure

## Próxima frontera

La integración, serialización, rollback y límites ya tienen pruebas XI.
La [campaña grande](integrated-scale-campaign.md) distingue profundidad,
entrelazamiento y crecimiento de coeficientes. Las siguientes fronteras son
la partición general de Reduced, el control de flujo y comparaciones externas
adicionales bajo la misma tarea y precisión.
