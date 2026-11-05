# RXE v1: perfil de bytecode XI de REVERSE

RXE1 se conserva sin cambios. RXE2 añade aplicaciones de conversación con
programa y memoria inicial: [BBY Q como ejecutable](BBYQ_RXE_APP.md).

`.rxe` ocupa el papel del archivo ejecutable dentro de REVERSE. Es una imagen
binaria para la ABI XI Machine v1: cabecera, nombre, tabla de constantes,
instrucciones numéricas, operandos tipados y saltos resueltos. La fuente `.xi`
se compila a esa representación; el archivo no contiene el script original.
Cambiar de extensión un texto no produce un ejecutable válido.

El cargador comprueba toda la imagen y convierte las instrucciones validadas
a XI canónico antes de llamar **una vez al QuantumRuntime original**. Es una
primera implementación con representación intermedia textual durante la carga,
no un segundo intérprete ni despacho directo de bytecode. Se conserva la única
autoridad InfiniteCore/Ether/InverseMemory, sus presupuestos, RNG y rollback.
No se promete una aceleración por guardar el programa en binario.

## Crear, inspeccionar y ejecutar

```sh
reverse_pc compile examples/hola-reverse.xi Hola-REVERSE.rxe "Hola REVERSE"
reverse_pc inspect-rxe Hola-REVERSE.rxe
reverse_pc init initial.rvp
reverse_pc run Hola-REVERSE.rxe initial.rvp after.rvp
reverse_pc disassemble Hola-REVERSE.rxe recovered.xi
```

El ejemplo calcula 6×7=42. RVPC2 queda idéntico porque el programa no escribe
memoria ni aplica puertas. El mismo formato admite registros, aritmética,
ciclos, memoria y las operaciones híbridas del perfil XI. Compilar e importar
no ejecutan el programa. Una ejecución fallida no publica salida ni estado.

En el escritorio: **XI Studio → Crear .RXE** genera un archivo en Aplicaciones.
Se abre desde Aplicaciones o Archivos y se puede anclar a escritorio/Inicio,
buscar, exportar, importar o enviar a la papelera. Al recuperar una ventana
después de reiniciar se muestra su última salida y se espera una ejecución
explícita. Los programas comparten el RVPC2 existente; no hay procesos o memoria
aislados por aplicación en esta ABI.

## Cabecera

Enteros little-endian, sin padding implícito. Tamaño fijo: 56 bytes.

| Offset | Bytes | Campo |
|---:|---:|---|
| 0 | 8 | Magic `52 58 45 31 0D 0A 1A 0A` |
| 8 | 2 | Versión de formato: 1 |
| 10 | 2 | ABI XI Machine: 1 |
| 12 | 4 | Tamaño de cabecera: 56 |
| 16 | 4 | Tamaño total exacto del archivo |
| 20 | 4 | Número de instrucciones |
| 24 | 4 | Número de constantes |
| 28 | 4 | Bytes UTF-8 del nombre |
| 32 | 4 | Entrada: 0 en esta versión |
| 36 | 4 | Indicadores reservados: 0 |
| 40 | 8 | FNV-1a64 de todo el archivo excepto este campo |
| 48 | 4 | Líneas de la fuente original |
| 52 | 4 | Reservado: 0 |

Después vienen el nombre UTF-8, las constantes y el código. Cada constante
contiene longitud `u32` y sus bytes UTF-8. Cada instrucción contiene línea
original `u32`, opcode `u16`, cantidad de operandos `u16` y sus operandos.
La suma FNV detecta corrupción accidental; no autentica autor ni procedencia.

| Tag | Operando | Payload |
|---:|---|---|
| 0 | Constante | Índice `u32` |
| 1 | Registro | Índice `u8`, r0–r15 |
| 2 | Entero con signo | 64 bits en complemento a dos |
| 3 | Dirección dinámica | Registro `u8`, equivalente a `$rN` |
| 4 | Salto | Índice `u32` de una instrucción `label` |
| 5 | Pauli dinámico | Registro `u8`, seguido de carácter ASCII I/X/Y/Z |

Los IDs de opcode se fijan en `src/rxe.hpp`, `specs()`, y no se reordenan.
Los nombres de las etiquetas se guardan en constantes y los saltos se resuelven
a índices antes de escribir la imagen. La carga valida tipos, registros,
referencias, líneas crecientes, etiquetas únicas y límites de cada instrucción.
Se conservan las instrucciones `label`, por lo que los ciclos mantienen el
conteo de pasos de la máquina textual.

## Límites y estado actual

- Imagen: 8 MiB; nombre: 160 bytes UTF-8; fuente expandida: 16 MiB.
- Hasta 131.072 instrucciones, 65.536 constantes, 524.288 operandos totales y
  4.096 por instrucción; constante individual: 16 KiB.
- La ABI conserva un millón de pasos, 16 registros de 64 bits, salida acotada y
  los presupuestos del núcleo. Los campos o versiones desconocidos se rechazan.
- Es bytecode propio de REVERSE. No es PE/x86, ELF/ARM, ni un formato de Android.
  La aplicación Android transporta el entorno; los programas que este carga
  son `.rxe`. No se usa el instalador de APK para instalar estos programas.
- Esta versión cubre programas de consola y cómputo XI. No aporta aún una ABI de
  ventanas, bibliotecas dinámicas, syscalls de anfitrión ni aislamiento entre apps.
- Android aporta la interfaz anfitriona. El ejecutable puede usarse también
  mediante el CLI C++ de REVERSE con la misma ABI.

## Verificación

`tests/rxe_test.cpp` compara la ejecución textual/binaria, memoria, núcleo y RNG,
incluye un circuito de 128 qubits, 100 programas de enteros, truncaciones,
alteraciones con checksum recalculado, 500 mutaciones estructuradas, rechazo de
texto renombrado, bucles agotados y rollback tras un error tardío.
`tests/rxe_cli.py` comprueba compilación, ejecución, desensamblado y conservación
de archivos de salida cuando falla la carga. CI ejecuta Release y ASan/UBSan/LSan.
La prueba Android crea, abre, restaura, ancla, busca, exporta e importa `.rxe`
mediante la interfaz real y verifica la conservación de RVPC2.
