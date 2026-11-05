# REVERSE 2.4 — interfaz móvil y continuidad del motor

## Avance integrado del chat compartido

Se revisó el chat [Termina trabajo IA](https://chatgpt.com/share/6ae16dea-4410-83e9-8ee6-6bf96cf0b027?ogimg=plain).
Su cierre entrega REVERSE Control 2.2.0, el puente XI de ventanas y las
correcciones del intérprete E022. Las referencias verificadas son REVERSE-PC
`0d867806038e7394a980a76d489ce247bcab2dfc` y BBY-Q
`3081c04d1b8a9a89773091ac3b90b6d196e7ad57`. Esas referencias ya son antecedentes
de la integración W4u utilizada como base. No se repite el entrenamiento.

La edición final 2.2 del chat tiene una identidad RXE diferente de la prueba
inicial. Se verificó que ambas contienen exactamente el mismo bytecode y seed;
ahora ambas identidades están reconocidas por la migración guardada.

## Interfaz

- Hasta 760 px, una aplicación ocupa la superficie disponible. Inicio,
  Programas, BBY Q y Abiertas son controles táctiles persistentes. Volver regresa
  al inicio sin borrar el estado. Las otras aplicaciones quedan en segundo plano.
- Por encima de ese ancho se conservan ventanas movibles, sus posiciones,
  tamaños y barra de tareas. Se comprobó el cambio PC → móvil → PC.
- La APK propaga los márgenes del recorte/gestos y la presencia del teclado.
  El compositor de BBY Q mantiene su superficie al abrir el teclado. El botón
  Atrás de Android usa la navegación del shell antes de salir.
- El arranque y Workbench respetan la orientación elegida en Android, sin
  bloqueo horizontal. La rotación redimensiona la misma WebView sin recrear la
  actividad ni reiniciar sus programas.
- Inicio concentra Programas, Archivos, BBY Q y XI. Sistema queda en Ajustes.
  Se retiran accesos duplicados, frases de plantilla, widgets decorativos y el
  sembrado automático de `Hola-REVERSE.rxe`. Una copia de fábrica antigua se
  omite del catálogo solo si coincide exactamente con sus bytes conocidos;
  el archivo se conserva. Un programa modificado permanece visible.
- Notas sigue siendo el editor de documentos; calculadora, ayuda y módulos
  anteriores conservan compatibilidad interna. Los programas del usuario no se
  borran para limpiar el inicio. Los documentos de bienvenida solo se retiran
  si contenido, nombre y carpeta coinciden con la plantilla y no están en papelera.

La interfaz es HTML/CSS local dentro de la aplicación Android, con el puente
nativo existente. En PC tiene navegación/ventanas adaptativas; esta entrega no
incluye un instalador nativo Windows/macOS. La edición web aislada indica que
no está conectada al motor en vez de fabricar métricas del dispositivo.

## Qubits, T y Hamiltonianos

El perfil compilado del núcleo tiene **4.096 qubits activos**, hasta **1.024 por
componente**, 64 MiB de payload y direcciones de hasta 4.096 dígitos. Los datos
proceden de `Runtime.core().limits()` en la misma configuración fijada.
La interfaz separa el límite configurado de los qubits/componentes usados por
la sesión. Una sesión vacía no cambia la cifra de capacidad.

Se ejecutó un registro de **4.096 qubits independientes**, con H y T en cada
uno. El resultado real materializa 4.096 componentes y registra 4.096
instrucciones T. No es una afirmación de eficiencia para un circuito arbitrario
de 4.096 qubits entrelazados. El perfil se contrasta automáticamente con la
lectura nativa para que las cifras del producto no deriven de una etiqueta.

XI tiene Editor, Operadores y Hamiltonianos. Este último genera y ejecuta:

\[
H=J Z_0 Z_1+h(X_0+X_1),\qquad \Delta t=\pi/8.
\]

Los coeficientes son enteros entre −3 y 3 y se admiten 1–16 pasos. Cada paso
aplica, en ese orden, las exponenciales de ZZ, X₀ y X₁. H/CX/T/T† implementan
las rotaciones correspondientes. Con acoplamiento y campo simultáneos se
declara un producto de Trotter de primer orden, no la exponencial exacta de
un Hamiltoniano arbitrario. Se leen observables reales y se calcula la energía
a partir de esos resultados.

Los experimentos parten de un estado nuevo y **no sustituyen el RVPC2 de
trabajo**. La operación `experiment` del anfitrión ejecuta el programa en el
runtime nativo y devuelve sus observables/estadísticas, sin llamar a
`SessionStore.writeState`. El editor XI conserva su ejecución explícita sobre
la sesión de trabajo. No hay un simulador alternativo en JavaScript: el cálculo
denso de referencia vive exclusivamente en las pruebas.

## Memoria, firma y actualización

El RXE de BBY Q/W4u 0.1 se conserva **byte por byte** desde Control 2.3.
Cambiar el shell no cambia la identidad usada para su almacenamiento privado.
El constructor verifica bytecode, seed, fuente enlazada y ejecutable contra el
manifiesto congelado; si cambia el motor, exige versionarlo/migrarlo.
La procedencia del shell se registra por separado de la del agente.

Las dos ediciones 2.2 conocidas migran con comprobaciones byte por byte.
Se probó una cuenta aprendida de cinco manzanas en cada una. Se conservan las
sesiones anteriores y los ejecutables personalizados no se sustituyen.

La APK se muestra como **REVERSE**, versión 2.4.0, versionCode 240, paquete
`com.reversepc.control.workbench`. Usa la misma clave de Control 2.2/2.3 y puede
actualizarlos sin desinstalar. Sigue siendo un paquete distinto del Workbench
original: no puede leer los datos privados de otro paquete Android. No se
incluye la clave de firma en el código ni en su archivo de entrega.

## Evidencia local

- `docs/evidence/native-shell-ui.json`: seis anchos (360, 390, 412, 768, 1024 y
  1440), navegación, teclado, ejecución RXE real, restauración sin repetición,
  tamaños de ventanas y saneamiento de perfiles.
- `docs/evidence/quantum-workbench-native.json`: 60 combinaciones contrastadas
  con un oráculo denso que calcula directamente cosenos/senos de ZZ y X, sin
  reproducir la descomposición H/CX/T. Error máximo menor que 10⁻¹⁵.
- `docs/evidence/native-mobile-migration.json`: ambas ediciones 2.2 conservan la
  cuenta aprendida y la imagen W4u 2.3 permanece idéntica.
- Las 83 verificaciones del escritorio y 37 de W4u siguen pasando. El módulo
  legado X MARKET conserva sus diez interacciones fuera del inicio principal.
- Java e instrumentación compilan contra Android 35. El recorrido Android está
  preparado para retrato, actualización y arranque en frío.

Las pruebas de disposición/gestos se ejecutaron con Chromium y adaptadores de
transporte que llaman al XI/RXE real. No se ha lanzado esta APK en un teléfono
o emulador Android en esta validación. La instrumentación preparada y la CI no
se presentan como ejecutadas.

## Reproducción

```bash
g++ -std=c++17 -O2 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1 \
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc \
  tests/core-profile.cpp -o build/core-profile
node reverse-os-android/tests/quantum-workbench-native.cjs \
  build/reverse_pc build/core-profile build/quantum-workbench
NODE_PATH=/ruta/node_modules REVERSE_BROWSER=/ruta/chrome-headless-shell \
  node reverse-os-android/tests/native-shell-ui.cjs \
  build/reverse_pc build/bbyq-control build/core-profile build/mobile-ui
```

La fuente del agente sigue en BBY-Q `86b52437d851c7c6db8a80a1d2d4c3e5e2affb09`.
Las ramas siguen locales mientras la publicación a GitHub espera autorización.
