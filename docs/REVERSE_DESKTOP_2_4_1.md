# REVERSE 2.4.1 — escritorio PC horizontal

Esta revisión aplica la nueva dirección de interfaz: la APK aloja el escritorio
PC en horizontal. Sustituye la portada y la navegación móvil de 2.4.

## Superficie del escritorio

Al iniciar una sesión nueva se ven el fondo y la barra de tareas. Se retiran el
panel de qubits, las tarjetas de herramientas, los recientes y la cabecera de
la portada. No se abre ningún programa automáticamente. Aplicaciones y Buscar
permiten acceder a las herramientas; la barra conserva Archivos, Programas,
BBY Q y XI, el cambio de espacio, Sistema, reloj y Mostrar escritorio.

Todas las pantallas usan ventanas PC con minimizar, maximizar, restaurar,
arrastrar y redimensionar. Enfocar una ventana no minimiza las demás. Mostrar
escritorio oculta y restaura el mismo conjunto de ventanas.

El arranque, Workbench y los accesos Android anteriores declaran
`sensorLandscape`: admiten las dos posiciones horizontales y excluyen la vista
vertical. Workbench conserva su actividad/WebView al cambiar de orientación.
El tamaño visible de una ventana se ajusta al espacio disponible sin alterar
el tamaño guardado al abrir/cerrar el teclado. Los márgenes laterales del
recorte de pantalla también se entregan al escritorio.

Atrás cierra un diálogo o menú abierto. Con el escritorio despejado, devuelve
el control a Android; no minimiza todas las ventanas ni abre una portada móvil.

## Programas y continuidad

Operadores, registro de qubits y Hamiltonianos siguen dentro de XI. Sistema
presenta las capacidades como filas del diagnóstico, junto al uso real de la
sesión. El diagnóstico Android usa sus lecturas nativas y no crea un contexto
WebGL para inspeccionar el navegador.

El RXE de BBY Q/W4u y las bibliotecas JNI ARM64/x86_64 permanecen idénticos a
2.4. La identidad de almacenamiento del agente se conserva. Siguen vigentes
las migraciones comprobadas de Control 2.2 y los archivos/perfiles de trabajo.
El refinamiento visual no reentrena ni reemplaza el motor XI.

La APK se muestra como REVERSE 2.4.1, versionCode 241, paquete
`com.reversepc.control.workbench`. Se firma con la misma clave retenida de
Control 2.2/2.3/2.4 para instalarla como actualización, sin desinstalar.

## Validación

- 98 comprobaciones de interfaz en seis superficies horizontales: 640×360,
  740×360, 844×390, 915×412, 1024×600 y 1440×900. Incluyen ventanas simultáneas,
  controles de la barra, arrastre, tamaño, teclado, recortes y restauración.
- W4u ejecutó una vez un RXE interno con el motor real; recargar la interfaz
  no repitió la acción.
- Se ejecutaron Ising y el registro de 4096 qubits independientes dentro de
  XI. Ambos conservaron el hash de la sesión de trabajo.
- Siguen pasando las 83 comprobaciones del control del escritorio y las 37
  de integración W4u.
- Java principal e instrumentación compilan contra Android 35. El manifiesto,
  firma, alineación, ABI y activos empaquetados se comprueban en la APK final.

Evidencia de interfaz: `docs/evidence/desktop-shell-ui.json`.
Las interacciones se ejecutaron con Chromium, transporte de prueba y el XI/RXE
real. No se ha lanzado la APK en un teléfono ni emulador Android. La orientación
se comprueba en el manifiesto compilado; no se presenta la instrumentación como
ejecutada. La validación física de 2.4 continúa documentada en
`REVERSE_NATIVE_2_4.md`; su diseño de portada/vertical ha quedado sustituido.

## Reproducir la interfaz

```bash
NODE_PATH=/ruta/node_modules REVERSE_BROWSER=/ruta/chrome-headless-shell \
  node reverse-os-android/tests/native-shell-ui.cjs \
  build/reverse_pc build/bbyq-control build/core-profile build/desktop-ui
```

La entrega conserva el núcleo y el contrato W4u anteriores. No contiene un
instalador Windows/macOS. Los cambios siguen en la rama local mientras la
publicación en GitHub queda pendiente.
