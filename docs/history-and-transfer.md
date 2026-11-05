# Historial y transferencia a Reduced — 25 octubre 2026

## Cambios

`record()` cancela pares adyacentes X/X, Z/Z, CX/CX y CZ/CZ sobre los mismos
operandos. Se ejecutan las dos instrucciones originales y `gate_count()`
las sigue contando; solo se compacta el registro usado para replay. No se
cancelan H/H ni T/T†, porque sus operaciones pueden introducir redondeo.
No se reordenan puertas ni se cambia una frontera transaccional.

Cuando el estado previo al fallback contiene exclusivamente bloques de un
qubit, `Reduced::from_product` importa sus amplitudes, fase y norma actuales.
Así la latencia no depende de la longitud previa del programa. Si el producto
no cabe en el límite de componentes se intenta el replay canónico anterior:
una base estabilizadora distinta podría necesitar menos componentes. Los
bloques entrelazados conservan siempre ese replay. El estado se confirma solo
tras completar conversión y puerta. Se liberan, con swap, las capacidades del
historial, bloques y mapa de propietarios que ya no son necesarios.

## Corrección explícita de compatibilidad

La fábrica `from_product` del commit fijado creaba el objeto con el límite
predeterminado ilimitado: comprobaba el límite durante la importación, pero
no lo transmitía al objeto que ejecutaría las puertas futuras. El nuevo
`scripts/prepare-ether-compat.py` verifica el SHA-256 completo del encabezado
y genera en el directorio de compilación una extensión local de **una línea**:
`out(product.size(), max_components)`.

No se modifica ningún archivo del checkout de Mundo X ni su commit fijado.
La extensión compilada sí contiene esta corrección explícita y se publica
como `ether-compat.diff` en CI. Fallan la configuración ante fuentes diferentes
y las pruebas si una T posterior deja de respetar el límite importado.
El encabezado AdaptiveReduced puede seguir compilandose sin la extensión;
en ese caso conserva el replay en vez de habilitar la importación.

## Mediciones reproducibles

21 procesos, orden alternado y un calentamiento. Baseline preservado
`e8a04e029adbeae3fc9bd963374d93d61f24f524`, mismo código de prueba,
GCC 13.3 -O3. Para history se mide el programa completo; para replay se
mide **solo la puerta que fuerza el fallback**, con la preparación fuera.
El caso no cancelable usa 20.000 S, no eliminadas del log; verifica que la
importación evita replay incluso cuando la compactación del historial no ayuda.

| Caso | Antes ms mediana / p95 | Después ms mediana / p95 | Payload estimado bytes antes / después | Pico RSS KiB antes / después |
| --- | ---: | ---: | ---: | ---: |
| history | 1.849234 / 3.701089 | 0.486055 / 0.683050 | 1573248 / 504 | 3288 / 1732 |
| replay | 22.421068 / 25.815123 | 0.025337 / 0.029786 | 240347 / 437 | 2732 / 1784 |
| replay_noncancelling | 22.206059 / 27.195644 | 0.038205 / 0.060511 | 240347 / 437 | 2736 / 2440 |

Datos de todos los circuitos: [history-transfer-local.json](measurements/history-transfer-local.json).
RSS procede de VmHWM del proceso después de exec; no se identifica el tamaño
lógico del espacio de direcciones con memoria física. El payload previo
al fallback del código antiguo omitía capacidades retenidas después de
`clear()`; por eso no debe interpretarse como consumo completo del proceso.

## Validación y límites

Diez suites Release, incluidas las pruebas anteriores y la auditoría PR #5;
200.000 operaciones de identidad, gate_count conservado, historial no cancelable,
comparación por puerta contra el oráculo independiente, importación de
productos y fallback entrelazado, rechazo atómico por límites y liberación de
capacidades. ASan/UBSan sobre ocho suites de corrección; LeakSanitizer en CI.

No se elimina todo historial posible: programas largos sin cancelación y sin
fallback siguen reteniéndolo. No existe aún importación general de bloques
entrelazados al Reduced fijado. La importación puede cambiar los últimos bits
de redondeo frente a reejecutar el circuito; los errores complejos se verifican
con el mismo umbral 2e-8 de la batería previa, sin poda de amplitudes.
Los logs internos del Reduced canónico tampoco se compactan aquí.

Siguiente: evitar copias repetidas de metadatos dentro del candidato privado de
un programa XI, conservando revisiones por instrucción, RNG, salida, snapshots
y rollback conjunto con InverseMemory. Después, definir un contrato único de
estados/observadores para incorporar AdaptiveReduced sin dos autoridades
cuánticas contradictorias.
