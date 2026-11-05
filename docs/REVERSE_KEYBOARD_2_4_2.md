# REVERSE 2.4.2 — teclado en el escritorio horizontal

Esta revisión corrige la integración del teclado manteniendo el escritorio PC.
La orientación continúa siendo `sensorLandscape`; no se añade una portada móvil.

## Correcciones

- `WorkbenchWebView` conserva el InputConnection original y solicita
  `IME_FLAG_NO_FULLSCREEN`. Evita pedir el editor extraído del IME; no impone
  ASCII ni cambia las acciones y el tipo de entrada de los campos.
- El contenedor nativo reserva la altura del IME una vez. Antes de propagar los
  insets a WebView, pone a cero el tipo IME ya aplicado. Sigue enviando las
  notificaciones al ocultarse, para que no quede espacio residual. Android 10
  tiene una alternativa basada en el rectángulo visible de la ventana.
- Se distingue entre teclado visible y teclado acoplado con altura. Un teclado
  flotante no reduce la superficie del escritorio.
- Durante la escritura con un teclado acoplado se oculta temporalmente la barra
  de tareas. Se ajusta la superficie visible de las ventanas sin cambiar sus
  coordenadas ni tamaños guardados. Los campos de BBY Q, Notas, XI y los diálogos
  aprovechan la altura disponible.
- El icono de teclado en el marco de la ventana o el diálogo permite ocultarlo.
  Atrás primero oculta el IME. Estos controles no cierran el programa ni borran
  su texto. Al ocultarlo vuelve la barra de tareas.
- BBY Q actualiza el historial y los controles sin desconectar su textarea.
  Se conserva el foco y la selección cuando llega una respuesta. Enviar limpia
  el mensaje enviado y permite preparar el siguiente mientras trabaja el motor;
  una respuesta no borra ese nuevo borrador. Ante un resultado incierto se
  conserva la protección existente que exige actualizar antes de repetir.

El ajuste sigue las referencias de Android sobre
[insets de WebView](https://developer.android.com/develop/ui/views/layout/webapps/understand-window-insets)
y [opciones de EditorInfo](https://developer.android.com/reference/android/view/inputmethod/EditorInfo).
La petición de evitar pantalla completa depende de que el IME la respete.

## Validación y alcance

- **43 comprobaciones de teclado**: respuesta XI real demorada para escribir un
  nuevo borrador durante el cálculo; continuidad del elemento, foco, selección,
  acentos y eñe; alturas disponibles de 260, 200, 160 y 132 px; ocultar, Atrás,
  teclado flotante, notas, fuente XI y diálogo de nombre.
- **99 comprobaciones del escritorio**, en los seis tamaños horizontales de
  la revisión anterior. W4u ejecutó una vez el RXE interno, sin repetición al
  recargar; los experimentos XI siguieron conservando la sesión de trabajo.
- **83 comprobaciones del control del escritorio**, con 19 acciones nativas.
- Java principal e instrumentación compilados contra Android 35. El recorrido
  Android preparado abre el IME, comprueba EditorInfo, escribe `á`/`ñ` mediante
  InputConnection, verifica que el espacio no se descuente dos veces y oculta
  el teclado desde el marco. No se presenta ese recorrido como ejecutado.

Evidencia: `docs/evidence/keyboard-ui.json` y
`docs/evidence/desktop-shell-ui.json`. Las pruebas del navegador usan insets
simulados y el XI/RXE real. **No se ha ejecutado esta APK en un teléfono ni
emulador Android**. No se afirma haber comprobado el IME o WebView concreto
instalado en el teléfono del usuario.

## Actualización

REVERSE 2.4.2, versionCode 242, conserva
`com.reversepc.control.workbench`, la firma retenida y el RXE de BBY Q/W4u.
Se instala como actualización de Control 2.2/2.3/2.4/2.4.1, sin desinstalar.
Las bibliotecas JNI y la identidad de memoria del agente permanecen iguales.

```bash
NODE_PATH=/ruta/node_modules REVERSE_BROWSER=/ruta/chrome-headless-shell \
  node reverse-os-android/tests/keyboard-ui.cjs \
  build/reverse_pc build/bbyq-control build/core-profile build/keyboard-ui
```

Los cambios siguen en la rama local; su publicación en GitHub queda pendiente.
