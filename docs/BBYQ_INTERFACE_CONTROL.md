# BBY Q controla REVERSE Workbench

La coordinación de agentes y ejecución de programas de la versión siguiente se
describe en [W4u 0.1](W4U_BBYQ.md). Esta página conserva el alcance de Control-1.

Esta primera integración conecta BBY Q E022 y su intérprete corregido con las
ventanas de REVERSE. La interpretación de las órdenes y la selección de una
operación ocurren en XI, dentro del mismo RVPC2 privado del agente. La interfaz
Android/JavaScript transporta observaciones y ejecuta operaciones concretas;
no contiene otro modelo ni un analizador de órdenes en lenguaje natural.

## Uso

Abre **BBY Q** y escribe una orden. La salida se configura en Español/English.

| Orden | Resultado |
|---|---|
| `abre archivos` | Abre Archivos o enfoca su ventana existente |
| `puedes abrir la calculadora` | Abre la calculadora |
| `abre centro de control` | Abre el centro de control de REVERSE |
| `minimiza calculadora` | Minimiza esa ventana |
| `maximiza archivos` | Maximiza Archivos |
| `restaura archivos` | Recupera su tamaño normal y la enfoca |
| `cierra esta ventana` | Cierra la ventana activa |
| `ordena las ventanas` | Ordena las ventanas del espacio actual |
| `muestra el escritorio` | Minimiza sus ventanas |
| `cambia a enfoque` | Cambia al segundo espacio |
| `open files` / `close files` | Las mismas acciones con órdenes inglesas |

Son nueve operaciones, 33 construcciones y 37 alias de herramientas/espacios.
La gramática de órdenes es una extensión diseñada y declarativa, con 77 formas
léxicas; no se presenta como un entrenamiento nuevo de conversación general.
La memoria E022 se conserva y no se repiten sus campañas anteriores.

## Flujo y confirmación

El anfitrión entrega al agente el número de ventanas por herramienta y la
herramienta activa. XI compara toda la orden con las tablas de su región
`A=8200000000000000000`; una orden incompleta, negada, compuesta no admitida o
con un destino desconocido no produce una acción. Para enfocar o cerrar una
herramienta deben existir referencias suficientes. Varias ventanas con el mismo
nombre requieren una selección más precisa.

El primer texto es «Voy a intentarlo.». El estado del agente y la petición se
guardan juntos antes de devolver el ticket. El escritorio verifica que su vista
no haya cambiado, aplica una operación del catálogo y guarda su perfil. Solo
después envía la confirmación a XI: «Acción completada.» o «No se completó la
acción.». Las confirmaciones también se guardan con el historial.

Cada ticket se consume una sola vez. Reabrir la ventana, cargar un catálogo o
reiniciar lee la conversación sin volver a ejecutar acciones. Si la aplicación
se cierra entre aplicar y confirmar, el resultado queda sin confirmar y no se
reintenta solo. Las órdenes de interfaz invalidan el referente factual activo;
una pregunta posterior no explica una afirmación vieja como si fuera una acción.
Los hechos, cuentas y temas aprendidos permanecen disponibles.
La referencia de la vista usa identidad, espacio y visibilidad; un cambio de
tamaño causado por el teclado Android no invalida la ventana de destino.

El código de interfaz acepta acciones únicamente del ejecutable exacto incluido
en la APK. Otro `.rxe` puede seguir usando su propia conversación/cálculo, pero
sus bytes no habilitan el control del escritorio. Esta integración opera sobre
las ventanas de REVERSE; no añade acceso de accesibilidad a otras apps Android.

## Memoria y actualización

El estado continúa el checkpoint E022 auténtico de 500.410 bytes, SHA-256
`33a2490be0d7a9252222991da1cd376e7cfb4ecb0014987c65d1b8b24278f20f`.
Las tablas de interfaz añaden 34.221 bytes serializados: 534.631 bytes en total.
La referencia fuente, el bytecode y los digests se registran en el manifiesto
del constructor. Mundo X permanece en `e2778d962af22f6f1a0794058dd4685fc2573966`.

Una actualización de la misma aplicación sustituye únicamente el BBY Q E016
original conocido. Primero aplica una migración XI que comprueba los 13.054
bytes de diferencia, conserva el núcleo, enlaces y datos del usuario fuera de
esa diferencia, y copia idioma e historial bajo bloqueo. Después publica el
ejecutable nuevo. Un conflicto aborta antes de reemplazar los datos; una memoria
nueva ya existente no se sobrescribe. El estado E016 anterior queda conservado
como respaldo privado. Un ejecutable personalizado o eliminado no se reemplaza.

La APK local de prueba usa el paquete separado
`com.reversepc.control.workbench`, llamado **REVERSE Control**. Se instala junto
al Workbench anterior y comienza con su propio perfil. La migración entre
versiones se comprobó con una cuenta E016 real en el runtime; esta edición
paralela no puede leer los datos privados de otro paquete Android.

## Validación local

- **839 comprobaciones nativas:** 803 combinaciones de construcciones y alias,
  19 controles de entradas no ejecutables, falta/ambigüedad de ventanas,
  conversación, separación de referentes y migración de una cuenta E016 que
  conserva cinco manzanas. Una migración con conflicto se rechaza.
- **77 comprobaciones del escritorio:** se usan las funciones de ventanas
  originales. Diecisiete acciones recorren el ejecutable XI real, la ventana y la
  confirmación en ES/EN. Se prueban tickets repetidos, una vista cambiada,
  error de guardado, conservación de notas, edición concurrente y redimensión
  sin cambio de identidad, además de los diez destinos de herramientas.
- Compilación de todas las clases Java contra Android 35; creación DEX,
  empaquetado ARM64/x86_64, alineación y verificación criptográfica de la APK.
- Se conserva la prueba de X MARKET y sus diez interacciones.

Las pruebas del escritorio usan fixtures para dibujar DOM y para el almacenamiento
Android. No se ejecutó esta APK en un teléfono/emulador Android durante esta
validación. No son una evaluación de conversación general ni una medición de
rendimiento Android. No hay ejecución de órdenes arbitrarias, borrado de archivos,
planificación autónoma general ni lectura visual de toda la pantalla. Persisten
255 bytes y 15 palabras por turno.

## Reproducir

```bash
# BBY-Q E022 fijado en e8498b7, con el parche de intérprete de esta carpeta.
git -C /ruta/BBY-Q apply "$PWD/bbyq-interface/patches/e022-parser.patch"
node bbyq-interface/build.mjs /ruta/reverse_pc /ruta/BBY-Q /ruta/salida /ruta/BBY-Q-E016.rxe --install-assets
node bbyq-interface/tests/native-control.mjs /ruta/reverse_pc /ruta/salida
node bbyq-interface/tests/desktop-control.cjs /ruta/reverse_pc /ruta/salida
```

El workflow de Workbench prepara esas fuentes fijadas, reconstruye el paquete
y mantiene el recorrido Android. `build-test-apk.sh` permite construir la edición
paralela local con build-tools 35, Android 35, una APK previa del propio proyecto
y una clave de desarrollo retenida. Reutiliza sin modificar las bibliotecas JNI
firmadas de esa APK, recompilando Java, recursos, interfaz y el ejecutable BBY Q.
