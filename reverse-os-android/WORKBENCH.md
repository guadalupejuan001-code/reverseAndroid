# REVERSE Workbench 2.1.1 para Android

## X MARKET instalado en Inicio

X MARKET se integra en Inicio → Aplicaciones, búsqueda, acceso del escritorio, Centro de control y barra de tareas junto a BBY Q. Incluye cuatro pantallas: Mercado (sin cotizaciones hasta conectar fuentes), Laplace (escenario matemático sintético), Experimentos y Configuración. Todas son interfaces locales. El gráfico de Laplace usa solamente la regla y los resultados verificados en el experimento XI del PR #27; abrirlo no ejecuta operaciones, no invoca el núcleo, no altera BBY Q ni crea señales bursátiles.

Esta versión conserva `com.reversepc.workbench`, firma de desarrollo y estado persistido; eleva `versionCode` a 211. Consulta [X MARKET](X_MARKET.md) para arquitectura, límites y pruebas.



Edición instalable `com.reversepc.workbench`, compatible con ARM64 y x86_64. Se instala junto a las ediciones anteriores y conserva un perfil propio. Usa el arranque REVERSE y el escritorio diseñado con el fondo Pliegue, papelera, ventanas, editor de fondo y centro de control.

## Pantalla y fondo

El modo inmersivo ocupa toda la pantalla, sin reservar franjas para los botones de Android ni para el recorte de cámara. Deslizar desde el borde permite mostrar temporalmente los controles del sistema. El teclado conserva espacio para escribir.

El editor del fondo está en Ajustes → Fondo de escritorio y Centro de control → Apariencia. No está anclado ni aparece como acceso en la barra de tareas, y no se abre automáticamente. El fondo Pliegue y las imágenes propias se desplazan y giran suavemente mediante una animación del compositor; Movimiento del fondo permite pausarlo. La animación se pausa al salir de la aplicación.

## Qué se ejecuta realmente

- El escritorio se dibuja con WebView a partir de archivos incluidos en la APK. No descarga la interfaz ni necesita un servidor.
- La calculadora compila expresiones con `CalculatorCompiler` y las ejecuta mediante `NativeRuntime.execute` en XI. No usa el evaluador JavaScript para producir los resultados nativos.
- XI Studio ejecuta los programas escritos por el usuario en el runtime original. El ejemplo calcula 6×7 y aplica H/CNOT/T a q200–q327; cambia el estado y se ejecuta únicamente al pulsar Ejecutar.
- Sistema consulta `NativeRuntime.inspect` sobre el mismo checkpoint `session.rvp` (RVPC2): qubits activos, componentes, bytes de tableaux, reglas Ether, excepciones/enlaces y revisiones.
- Las lecturas de dispositivo, memoria y CPU acumulada del proceso proceden de Android. La CPU acumulada no es un porcentaje de carga total. Las cifras del núcleo son datos de una arquitectura de software, no mediciones de hardware cuántico físico.
- Aplicaciones .rxe, los accesos de BBY Q, el buscador y el Centro de control abren ventanas dentro de Workbench. No cierran la mesa ni lanzan DesktopActivity. BBY Q conserva el mismo ejecutable E016 y su memoria privada por SHA-256; no se modificó su entrenamiento.
- Una tarea de DesktopActivity que Android restaure desde Recientes después de actualizar redirige a Workbench antes de dibujar el escritorio anterior o leer sus datos.
- El catálogo permite agregar y exportar los binarios RXE1/RXE2. XI Studio puede compilar un .rxe sin ejecutarlo. Los programas RXE1 se ejecutan al pulsar Ejecutar; al restaurar una ventana solo se carga su última salida.
- La conversación de BBY Q usa RxeAppStore/NativeRuntime.appTurn y publica historial y RVPC2 de forma atómica antes de mostrar una respuesta. Las copias idénticas del ejecutable comparten memoria. Memoria .rvp exporta ese checkpoint real.

`RuntimeExecutor` serializa las transacciones nativas de Workbench, DesktopActivity y MainActivity en una sola cola de proceso. Las interfaces cargan el estado confirmado, ejecutan y guardan antes de devolver un resultado. La dependencia Mundo X permanece fijada a `e2778d962af22f6f1a0794058dd4685fc2573966`.

## Datos y persistencia

El estado del núcleo usa `SessionStore`/RVPC2. Los documentos de la nueva mesa, preferencias y ventanas usan `workbench-profile.json`, escrito con `AtomicFile` y confirmado a la interfaz. La copia en `localStorage` es secundaria; al arrancar se prefiere la copia nativa. La restauración de un respaldo de la mesa sigue validando el JSON y pidiendo confirmación. Ese JSON contiene documentos, preferencias y ventanas; los binarios y memorias RXE se exportan desde sus ventanas. La actualización 2.0 → 2.1 mantiene com.reversepc.workbench, aumenta versionCode de 200 a 210 y conserva la firma de desarrollo para actualizar sin borrar datos.

Las exportaciones usan el selector de documentos de Android y confirman el éxito después de escribir. Las importaciones usan el selector de archivos. El WebView con puente solo sirve los archivos propios bajo `https://reverse.local/`; bloquea navegación de terceros, subframes, conexiones y acceso `file://`. Los enlaces externos se abren en un navegador separado sin puente.

El fondo aprobado se empaqueta en WebP para reducir su tamaño; la imagen original se conserva en el proyecto de diseño. `build-info.json` registra el commit exacto del código usado por la compilación.

## Compilación y pruebas

Con las mismas dependencias fijadas y la firma de desarrollo del proyecto:

```sh
gradle -p reverse-os-android :app:assembleWorkbenchDebug :app:assembleWorkbenchDebugAndroidTest
```

El workflow `workbench-android.yml` compila y verifica firma/ABI. Luego instala la APK en Android 15 y ejecuta `WorkbenchTestRunner`: lecturas JNI/Android, aritmética entera/decimal, rechazo de división por cero, operaciones de 128 qubits, calculadora desde la interfaz, papelera, preferencias y conservación después de cerrar forzadamente el proceso. Además instala la APK 2.0 publicada, crea una conversación y documentos, y aplica 2.1 con adb install -r. Verifica por hash que el agente guardado sobrevive antes de enviar otro turno. Pulsa las entradas de Aplicaciones, Centro de control y BBY Q con eventos táctiles y vigila que DesktopActivity nunca se abra. Comprueba conversación ES/EN, RXE1, compilación sin ejecución, importación/exportación binaria, rechazo de texto renombrado y restauración de las ventanas sin reejecutar. Repite el recorrido con una instalación limpia. Conserva APK, registros y capturas. No sustituye una prueba en un Samsung A16 físico.

Esta edición continúa siendo un entorno REVERSE alojado sobre Android; no reemplaza el kernel ni los controladores del teléfono.

El emulador de CI marca el aviso educativo de pantalla completa como confirmado, igual que [WindowInsetsBehaviorTests de AOSP CTS](https://android.googlesource.com/platform/cts/+/master/tests/tests/systemui/src/android/systemui/cts/WindowInsetsBehaviorTests.java). Esto evita que una ventana del sistema intercepte los toques de la instrumentación. La APK no cambia ese ajuste del teléfono.
