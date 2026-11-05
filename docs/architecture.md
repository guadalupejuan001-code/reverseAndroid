# Arquitectura integrada de REVERSE-PC

La autoridad cuántica es InfiniteCore: una tabla de direcciones y componentes
con un único payload activo por componente. CompactCore representa Clifford;
AdaptiveReduced conserva factores pequeños; Reduced admite estados
no-Clifford anchos. Ether define el marco global y propaga observables sobre
esa misma autoridad. InverseMemory conserva reglas clásicas y enlaces, no
una copia del registro cuántico.

XI admite T/T†, observación y transacciones conjuntas. RVPC2 persiste la
revisión de memoria además de los datos, núcleo, RNG y Ether, y acepta RVPC1.
El [contrato actual](unified-authority.md) detalla transferencias, precisión,
presupuestos, compatibilidad y las capacidades que aún faltan.

## Diseño y experimento iniciales, conservados como historial


## La arquitectura que se está ensayando

**Estado.** Un único QuantumRuntime contiene dos fuentes de verdad:
InfiniteCore para transformaciones lógicas cuánticas de direcciones
decimales, e InverseMemory para bytes definidos por un fondo global,
excepciones finitas y asociaciones cuánticas explícitas.

**Ejecución.** Un programa XI transforma candidatos privados; ambos
estados se confirman solo si finaliza todo el programa correctamente.

**Observación.** Las órdenes mem_read, probz, expect, mem_stats y stats
leen partes del estado. Los bytes no deterministas se devuelven mediante
identificadores sin mediciones implícitas.

**Persistencia.** REVERSE-PC guarda ambas instantáneas reales en un
único archivo RVPC1. Un checksum FNV-1a detecta errores accidentales,
pero no aporta autenticación criptográfica. El archivo temporal se
renombra tras la escritura completa.

## La experiencia de dos programas

Inicio:
- mem_store 7 5 y mem_store 8 10 (datos clásicos)
- mem_bind 9 100 (byte explícitamente vinculado a q100..q107)
- X100 y X102 (el byte 9 contiene el patrón binario 00000101)
- Consulta de una dirección lógica de 64 cifras (fondo implícito)

A aplica CNOT(100,101). Como bit0 era 1, cambia bit1 a 1:
00000101 -> 00000111, es decir, byte 9 = 7.

B aplica HAll. Los datos clásicos 7 y 8 no cambian, pero la nueva
regla global afecta a las probabilidades observadas incluso en
direcciones no materializadas. Un mem_read del byte vinculado ahora
devuelve un handle: no se le atribuye un valor clásico inexistente.

La rama inicial, A y B no comparten mutaciones, solo una instantánea
inicial idéntica. Las salidas observables, los archivos y los hashes
permiten comprobarlo externamente.

## Partes reales y pendientes

| Componente | Función real |
| --- | --- |
| QuantumRuntime | Transacciones conjuntas sobre núcleo y memoria |
| InfiniteCore | Puertas Clifford simuladas y consultas Pauli |
| EtherCore | Capas globales simbólicas admitidas (H y CZ) |
| InverseMemory | Direcciones lógicas y overrides sin byte array denso |
| REVERSE-PC | Programas, bifurcación, persistencia, comprobaciones |
| CPU del servidor | Ejecuta el código físico de todo lo anterior |

No interviene el renderizador. El estado es una representación
matemática simulada, no un nuevo campo físico.

## Qué falta para hablar de una computadora programable

1. Introducir operaciones en las que los bytes almacenados determinen
   la siguiente instrucción, el salto y la transformación aplicada.
2. Probar la ejecución de programas no hardcodeados, con control de
   flujo y una representación compacta del estado y sus reglas.
3. Contrastar la complejidad temporal y RAM residente de diferentes
   programas, en el mismo hardware, frente a algoritmos convencionales.
4. Diseñar y comprobar un contrato opcional con MIL/Reduced 1000
   sin confundir la capacidad de ese backend con la de InfiniteCore.
5. Construir una interfaz de observación solo cuando lo anterior funcione.

La demostración actual prueba una **separación funcional del estado,
la transformación y la observación**; no prueba una computadora
universal ni rendimiento no clásico.
