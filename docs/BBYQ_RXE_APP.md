# BBY Q como aplicación RXE2

`BBY-Q.rxe` contiene el programa E016 y su memoria entrenada auténtica.
Se abre como aplicación de conversación dentro de REVERSE Desktop RXE 1.2.
El archivo ocupa 302.774 bytes: 92.986 de bytecode, 209.659 de RVPC2 y
129 de cabecera/metadatos. No descarga modelos, reproduce el entrenamiento ni
incorpora un modelo de terceros.

## Procedencia y ejecución

- BBY Q fijado: `af1f9bff0e95cf78f0e03b5c289f7bd4dcdb9aa3`.
- Estado E016: SHA-256 `55992a06df9ec9cad6889f3dabbdbc179d2626cfd32667efec1f74b78660e732`.
- Código: enlace original `ai/xi/knowledge/link.sh conversation`, compilado a
  RXE1, con 5.113 instrucciones. No se reescribió la inteligencia en Java/C++.
- El contexto E016 fue entrenado con el runtime `6d4de14`; el núcleo y sus
  extensiones coinciden con los de la base del escritorio usada aquí.
- Mundo X conserva `e2778d962af22f6f1a0794058dd4685fc2573966` y sus extensiones
  verificadas. Cada turno entra una vez al QuantumRuntime original.

El perfil transporta UTF-8 a las mismas posiciones de memoria que
`BBY-Q/ai/native/session.cpp`: idioma en `7400000000000000003`, modo en
`+16`, longitud en `+17` y bytes a partir de `+128`, respecto de
`7400000000000000000`. Solo convierte los registros de salida r14/r9 a texto.
La tokenización, gramática, cuentas, razonamiento y construcción de respuestas
siguen íntegramente en los programas XI originales.

## Usar y empaquetar

En el escritorio: **Aplicaciones → BBY Q**, o el acceso BBY Q de Inicio.
Permite seleccionar Español/English y exportar su RVPC2 con **Memoria**.
**BBY Q anterior** conserva el acceso al estado E009 del escritorio.

```sh
python3 scripts/build-bbyq-app.py build/reverse_pc /ruta/BBY-Q BBY-Q.rxe
build/reverse_pc inspect-rxe BBY-Q.rxe
build/reverse_pc app-init BBY-Q.rxe bbyq.rvp
build/reverse_pc app-turn BBY-Q.rxe bbyq.rvp bbyq.rvp es "ana tiene dos manzanas"
build/reverse_pc app-turn BBY-Q.rxe bbyq.rvp bbyq.rvp es "ella recibe tres mas"
build/reverse_pc app-turn BBY-Q.rxe bbyq.rvp bbyq.rvp en "how many apples does she have?"
```

La primera respuesta cuenta dos manzanas; las siguientes cuentan cinco.
`app-init` se niega a sobrescribir una memoria existente. Para empaquetar
otros programas que usen este mismo buzón se puede usar `pack-app` del CLI.
El constructor específico de E016 exige su revisión y checkpoint comprobados.

## Memoria y transacciones

La memoria del archivo ejecutable es la semilla inmutable. La conversación
activa tiene **una sola autoridad RVPC2 por ejecutable**, separada del RVPC2
del escritorio; no se mezcla ni se borra el estado E009 anterior.

La identidad de datos es SHA-256 de todos los bytes del ejecutable. Copiarlo
o cambiarle el nombre conserva el agente; modificar su código o semilla crea
otro espacio, sin sobrescribir el anterior. Una nueva versión no migra por
sí sola la memoria de otra imagen.

`rxe-apps/<sha256>/session.rxas` guarda juntos RVPC2, idioma y hasta 80 mensajes,
en un paquete RXOS1 cuyo campo de texto contiene JSON de metadatos versión 1.
La publicación sincroniza el archivo temporal y lo renombra atómicamente.
Cada turno recarga el estado vigente bajo un bloqueo de escritor, de modo que
dos ventanas o la recreación de la actividad no pierden actualizaciones.
La respuesta solo aparece después de guardar. Un fallo nativo o de escritura
no publica la nueva respuesta ni reemplaza la sesión anterior.

Reabrir o recuperar una ventana lee su conversación sin ejecutar otro turno.
La exportación Memoria contiene el RVPC2 real. El respaldo ZIP del escritorio
incluye también `datos-aplicaciones/<sha256>/session.rxas`; la restauración
integral de ese ZIP sigue sin implementarse.

## Formato RXE2

Límite total 8 MiB. Todos los enteros son little-endian. Después de esta
cabecera de 64 bytes vienen id, título, versión y SHA de origen, seguidos de
la imagen RXE1 y el estado RVPC2, sin padding.

| Offset | Tipo | Campo |
|---:|---|---|
| 0 | 8 bytes | `RXE2\r\n\x1a\n` |
| 8 | u16 | Formato 2 |
| 10 | u16 | ABI 2, buzón de conversación BBY Q v1 |
| 12 | u32 | Cabecera: 64 |
| 16 | u32 | Longitud total exacta |
| 20, 24, 28 | 3 × u32 | Bytes de id, título y versión |
| 32, 36 | 2 × u32 | Bytes de código RXE1 y semilla RVPC2 |
| 40 | u64 | FNV-1a64 del archivo, excluyendo este campo |
| 48 | u32 | Entrada máxima: 255 bytes |
| 52 | u32 | Interfaz 1: conversación |
| 56 | u32 | Reservado: 0 |
| 60 | u32 | Referencia de origen: 40 bytes hexadecimales |

La carga comprueba dimensiones, campos conocidos, integridad de la imagen,
bytecode y restauración exacta de RVPC2. FNV detecta corrupción; no es firma
ni autentica el publicador. Los RXE1 de consola conservan su formato y ABI.

## Límites comprobados

E016 admite hasta 255 bytes/15 palabras por entrada, 16 cuentas por episodio y
cantidades 0–65.535. La preferencia ES/EN controla la salida; el perfil no añade
comprensión libre ni nuevas capacidades al modelo. La respuesta del contenedor
se acota a 4.096 bytes. La interfaz es conversación, no una ABI gráfica general.

La prueba nativa compara 64 turnos con el adaptador original, incluyendo cuatro
reinicios, respuestas exactas y RVPC2 idéntico tras cada turno. Se conservan
las pruebas RXE1 y del núcleo. Android verifica conversación real, contexto al
reiniciar, ES/EN, exportación e importación del ejecutable, memoria compartida
entre copias idénticas y rechazo transaccional ante un fallo real de escritura.
