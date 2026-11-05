# REVERSE Desktop RXE 1.2


## BBY Q como ejecutable

`BBY-Q.rxe` alberga BBY Q E016: su programa XI compilado y el checkpoint real
aprendido, en un archivo de 302.774 bytes. Se abre desde Aplicaciones o Inicio,
con conversación en español/inglés, contexto e historial persistentes y
exportación de memoria. Los turnos se ejecutan en XI/Ether; la interfaz no
incorpora otra IA. E016 mantiene diálogos acotados, no conversación abierta.

Su memoria activa y conversación se guardan juntas, sin modificar el RVPC2 del
escritorio. Una copia idéntica del ejecutable conserva el mismo agente. Inicio
conserva **BBY Q anterior** para el estado E009 previo. [Formato, procedencia,
transacciones y límites](../docs/BBYQ_RXE_APP.md).

## Programas propios .rxe

RXE es el formato ejecutable de REVERSE: cabecera, versión de formato y ABI,
constantes, instrucciones binarias y operandos tipados. El cargador valida todo
el archivo y entrega el programa a la máquina XI/Ether original. No se instala
ningún APK para agregar un programa `.rxe` a este entorno.

- **XI Studio → Crear .RXE** compila el código actual sin ejecutarlo y guarda
  el resultado en Aplicaciones. El código fuente `.xi` se conserva.
- **Aplicaciones → Agregar .rxe** valida e importa un ejecutable sin ejecutarlo.
- Abrir un `.rxe` desde Aplicaciones o Archivos lo ejecuta en una ventana de
  REVERSE. Se puede anclar a escritorio/Inicio, buscar, exportar y recuperar.
- Restaurar la sesión abre la ventana con la salida guardada; no repite la
  ejecución automáticamente. El botón Ejecutar permite repetirla expresamente.
- `Hola-REVERSE.rxe` viene como primer programa: calcula 6×7=42 y conserva RVPC2
  idéntico. Se conserva también una copia exportable generada por el CLI.
- Android sigue aportando la interfaz anfitriona. El botón Android conserva
  el lanzador de apps del teléfono que ya existía en 1.0.

[Especificación y límites de RXE](../docs/RXE_EXECUTABLES.md). Esta versión
ofrece programas de consola/cómputo XI: no añade todavía syscalls gráficas ni
memoria aislada por aplicación. Los programas usan el RVPC2 compartido; el motor,
Mundo X fijado y la versión de BBY Q se conservan.

La aplicación que contiene este entorno se distribuye como
`REVERSE-Desktop-RXE-v1.2.apk`, paquete `com.reversepc.desktop.rxe`, para instalarse
junto a 1.0 y conservarla. La clave antigua no fue retenida. Esta edición usa una
ruta de clave explícita y una caché de desarrollo; la caché puede caducar y no
sustituye un almacén de firma de producción. RVPC2 se puede exportar/importar con
Ajustes; el ZIP de respaldo conserva los documentos para extracción, sin una
restauración completa de perfil en esta versión.

CI conserva los recorridos de Desktop 1.0 y verifica creación/ejecución RXE,
restauración sin ejecución, accesos, búsqueda, exportación/importación binaria y
rechazo de un texto renombrado `.rxe`. Las pruebas C++ comparan estado, salida y
RNG con la ejecución textual y prueban corrupción, saltos, límites y rollback.

## Compilar la edición RXE

Con las dependencias fijadas del repositorio, JDK 17, Gradle 8.9 y el SDK/NDK
indicados en el workflow, ejecutar desde la raíz de REVERSE-PC. La configuración
de firma requiere preparar explícitamente la clave también en una máquina local:

```bash
export REVERSE_SIGNING_KEYSTORE="$PWD/build-signing/reverse-desktop.jks"
umask 077
mkdir -p "$(dirname "$REVERSE_SIGNING_KEYSTORE")"
if [ ! -f "$REVERSE_SIGNING_KEYSTORE" ]; then
  keytool -genkeypair -keystore "$REVERSE_SIGNING_KEYSTORE" -storetype JKS \
    -storepass android -keypass android -alias reverse-desktop \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=REVERSE Desktop Development"
fi
bash reverse-os-android/scripts/prepare-bbyq.sh deps/bbyq
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
gradle -p reverse-os-android :app:assembleRxeDebug
```

El resultado es `reverse-os-android/app/build/outputs/apk/rxe/debug/app-rxe-debug.apk`.
Conservar la misma clave para actualizar instalaciones firmadas con ella; no se
incluye en Git ni en los artefactos públicos.

## Base conservada de Desktop 1.0


Escritorio de trabajo dentro de Android, desarrollado sobre REVERSE OS v0.5
(`7420e067db6b42d207dd43eab00bdb9942eacafc`). Mantiene el diseño plano en grafito
con fondo orbital y el arranque `welcome.... REVERSE.`.

## Trabajo cotidiano

- Varias ventanas dentro de la misma actividad: mover, redimensionar, minimizar,
  maximizar, ajustar izquierda/derecha y recuperar cada tarea en la barra.
- Inicio, búsqueda de aplicaciones/documentos, escritorio y panel de ventanas.
- Hasta doce ventanas, con posiciones y sesión recordadas entre reinicios.
- Archivos: carpetas, texto, copiar archivos, mover archivos/carpetas, renombrar,
  papelera, restauración sin sobrescribir destinos existentes y eliminación
  definitiva con confirmación.
- Editor de texto UTF-8 y XI Studio con borradores conservados al cerrar o
  reiniciar. Los borradores no sustituyen el archivo hasta pulsar Guardar.
- Calculadora: precedencia, paréntesis, signo y seis decimales. La interfaz
  compila expresiones a XI; las operaciones se ejecutan en el intérprete propio.
  Usa registros de 64 bits y rechaza desbordamientos/división por cero.
- Visor de imágenes con decodificación acotada; navegador Android WebView;
  acceso explícito a las aplicaciones instaladas en Android.
- Preferencias de movimiento/calidad conservadas. El fondo se pausa cuando una
  ventana está maximizada, dos ventanas cubren ambos lados o Android suspende
  la actividad.
- Importar/exportar archivos mediante el selector Android, exportar respaldo
  ZIP de documentos/programas/memoria/borradores y restaurar RVPC2 validado.
  El ZIP se puede extraer con un gestor de archivos; no se importa como un
  perfil completo desde la interfaz de esta versión.
- Uso opcional como pantalla de inicio: Ajustes → Usar como escritorio de inicio.
  Android pide al usuario elegir la aplicación de inicio. Volver a Inicio
  recupera la misma actividad y no crea otra autoridad XI.

Atajos con teclado: Alt+Tab, Alt+F4, Ctrl+S, Ctrl+Alt+T, Meta+D y Meta+flechas.
Con el dedo/ratón, mantener pulsado un archivo o usar ⋮ abre sus operaciones;
el menú de cada ventana permite ajustar y restaurar su tamaño.

## Archivos y conservación

Los programas `.xi` continúan en la carpeta original `files/programs`; la
memoria computacional sigue siendo el mismo `files/session.rvp`. Las carpetas
Documentos, Descargas, Imágenes, Escritorio y Papelera viven en almacenamiento
privado. Android solo expone un archivo externo cuando el usuario lo elige.
No se solicita acceso general al almacenamiento ni se borran archivos al cerrar
ventanas. Los límites son 8 MiB por archivo, 256 KiB por texto/programa y 64 MiB
de documentos por respaldo ZIP. Las carpetas se mueven; copiar carpetas completas
no está incluido. Los archivos en papelera se pueden restaurar a su origen.

El gestor usa vistas no modales; desaparece la limitación anterior de un único
`Dialog` minimizado. Teclado y cambios de área disponible acotan las ventanas
sin perder su tamaño normal. Las barras se ocultan mientras se escribe para
liberar espacio en el teclado horizontal. La actividad es `singleTask` y el trabajador XI es
único en el proceso, también durante recreaciones de la pantalla.

Se mantiene la aplicación BBY Q ya incluida en v0.5, con sus fuentes E009
fijadas en `7dfa6c31247cf609128db78908313a6ce41c0b80`. Esta iteración no actualiza
su entrenamiento ni integra los checkpoints E014: ese trabajo permanece
separado, como se había acordado. La rama pendiente de v0.6 no se incorpora
silenciosamente.

## Instalar

`REVERSE-Desktop-v1.0.apk` usa `com.reversepc.desktop`, por lo que se instala al
lado de versiones antiguas de REVERSE OS y conserva su instalación. Es un
espacio nuevo; no puede leer el almacenamiento privado de otra aplicación.
Puede importar su RVPC2 desde Ajustes si se exportó previamente.

`REVERSE-OS-v1.0-compatible.apk` conserva `com.reversepc.os`. Solo puede actualizar
una instalación con la misma firma. No desinstalar una versión anterior para
resolver una firma distinta sin exportar antes sus datos. La compilación 1.0 no conservó la clave en la ruta prevista. Estas APK son compilaciones de desarrollo; no debe darse por compatible una firma nueva.

Compilar ambas ediciones históricas, después de preparar y exportar la ruta de
firma como se indica arriba:

```bash
bash reverse-os-android/scripts/prepare-bbyq.sh deps/bbyq
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
gradle -p reverse-os-android :app:assembleDesktopDebug :app:assembleClassicDebug
```

El runtime y Mundo X se conservan fijados. Android aporta kernel, controladores,
GPU, almacenamiento y WebView; XI/Ether ejecutan programas y conservan RVPC2.
Este proyecto aún no reemplaza el kernel Android ni ejecuta aplicaciones de
Windows. Las cifras de FPS/PSS provienen del proceso/renderizador reales;
no se presentan como aceleración cuántica ni como pruebas del Samsung A16.

## Verificación

`DesktopCoreTest.java` realiza operaciones reales con archivos, conflictos,
papelera, Unicode, límites, enlaces y geometría. `check-calculator.py` ejecuta
19 programas generados en el runtime XI y comprueba respuestas independientes,
rechazo y conservación del núcleo/memoria. Estas pruebas se ejecutan en host.

La CI además compila y firma las dos ABI (`arm64-v8a`, `x86_64`), instala la APK
Desktop en Android 15 y opera la interfaz mediante `adb`: arranque, movimiento
real del fondo, varias ventanas, arrastre, tamaño, ajuste, archivos, borradores,
calculadora XI, Euclides y recuperación tras matar/reabrir el proceso. La
batería ampliada además opera WebView, aplicaciones Android, el acceso Inicio,
importación de imágenes, exportación de borradores y respaldo ZIP a través del
selector real de Android. Guarda
capturas reales, registros y resultados. La prueba en el teléfono físico queda
fuera de lo que puede comprobar el emulador.
