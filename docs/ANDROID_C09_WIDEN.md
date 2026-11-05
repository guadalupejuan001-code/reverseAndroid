# Android C09: bloqueo de ampliación corregido; siguiente instrucción localizada

## Punto recuperado de C08

La ejecución 40982823290, artefacto 12945458506, había llegado a 6.450.570.741 transiciones ARM64, EL0 PC `0x4cc358`, antes de detenerse en `0x2f10a442`: `USHLL v2.4s, v2.4h, #0`. SHA-256 del estado: `547b7245def6e27a60201744a5837e8bbb0a4c2e9bf6eab4b2ecefa61dde9397`.

Su UART original confirma 19 módulos cargados, dispositivos virtio block/GPU enlazados, ocho particiones GPT, montaje correcto de `/metadata` y creación de particiones lógicas, incluidas `system_a`, `vendor_a` y `product_a`. Esto no demuestra el montaje de estos últimos sistemas de archivos, AVB completo, interfaz Android ni juegos.

## Corrección implementada

Se añadieron 13 líneas al intérprete original para SSHLL/USHLL y sus variantes de mitad superior. Q selecciona la mitad de origen; el resultado siempre ocupa 128 bits. Se preservan el origen cuando coincide con el destino, la extensión de signo, las restricciones de codificación y la comprobación original de acceso FP. No se sustituyeron el emulador, XI/Ether, la autoridad de memoria ni los dispositivos.

## Validación ejecutada

La ejecución de GitHub 40997177448, sobre el commit de fuente 871e7018d4808a2609c144b520395a8a0ce84529 y su merge de prueba 31a83f99bdb199ff41fe74c88f74abe0322514cb, aprobó:

- 70.675 comprobaciones UBSan nuevas, correspondientes a 1.792 casos de ampliación.
- Las 11.675 comprobaciones FP/SIMD existentes.
- Las 74 comprobaciones de ciclos, recuperación, prefijos de fallo y diario atómico.
- La identidad del estado original C08 y el rechazo de un informe alterado antes de abrir el disco.

Comprobaciones locales adicionales: 225 codificaciones contrastadas con LLVM, rechazo de cuatro entradas de evidencia alteradas y control negativo contra el intérprete original, que rechaza la nueva regresión. El parche se aplicó a una copia limpia de C08 y coincidieron los seis archivos de fuente probados. Estas pruebas unitarias no equivalen a arrancar Android.

## Resultado Android real: avance y nueva parada

La misma ejecución descargó el disco original y reanudó el estado C08, conservando los 128 MiB de RAM, kernel/ramdisk/disco, memoria XI/Ether y semilla/cadencia/fase del contador. Avanzó **5.682.588 transiciones reales del emulador**, hasta **6.456.253.329**, y se detuvo en otra instrucción no implementada:

`0x0e612804` en PC `0x4ce520`: **XTN v4.4h, v0.4s**.

El flujo terminó con fallo arquitectónico, no con éxito general. No hubo kernel panic ni nuevo texto UART en este tramo. El estado final fue guardado y su hash, cabecera, imagen inmutable y siguiente opcode se comprobaron de forma independiente tras descargar el artefacto. El código SIMD incluido en el artefacto coincide byte por byte con el probado localmente.

## Referencia exacta para continuar, sin volver a diagnosticar C08

- PR: 77, rama `fix/android-c09-widen-shifts-20261029`; no fusionada a main.
- Run: 40997177448; artefacto: **12951339528**, `reverse-c09-widen-continuation`.
- Estado `current.rvg` SHA-256: `773277104d567e2f72a6e1c66907dfcf5f87f17d77ee1b2dafcee64e87ab0ad3`.
- Informe `current.json` SHA-256: `2f50ba79ef1bcfd0838a3bbd344b92d749576671cd6f76e357159c91c73a6d0c`.
- PC virtual: `0x4ce520`; física: `0x43f2e520`; EL0; contador: 6.456.253.329; fase: 49 de 64.
- Disco original: artefacto 12912064693 del run 40910284394, 8.738.832.384 bytes, SHA-256 `28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c`.

Siguiente tarea: implementar y probar XTN/XTN2 en el intérprete existente, cubriendo reducción 16→8, 32→16 y 64→32 bits, preservación de la mitad baja en XTN2, limpieza de la mitad alta en XTN, registros coincidentes, codificaciones reservadas y acceso FP. Después, reanudar este estado C09 con sus pruebas de procedencia y la misma imagen de disco. No omitir la instrucción ni cambiar el estado para aparentar avance.

El framework, la pantalla de Android y los juegos siguen sin verificarse. Este resultado no es una APK Android lista ni justifica fusionar la cadena experimental en main.
