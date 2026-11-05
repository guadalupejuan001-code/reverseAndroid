# SPACE GLITCH V2 — APK nativa para Android

Emulador **visual** de distorsión espacial para experimentar con glitch, ondas, portales y espejos; no transforma materia ni es una simulación física.

- **GPU nativa:** OpenGL ES 3.0. Sin WebView, Internet ni dependencias de terceros.
- **4 mundos:** Portal, Fractura, Éter, Espejo.
- **Controles:** arrastrar para desplazar la cámara, pellizcar para ampliar, tocar para emitir una onda.
- **Interfaz:** intensidad de glitch, distorsión, velocidad, calidad ECO/AUTO/ULTRA.
- **Rendimiento:** render interno ajustable, resolución dinámica en AUTO, FPS medidos por fotogramas presentados al renderizador, no un FPS ficticio.
- **Captura PNG:** guarda en Imágenes/SpaceGlitch mediante MediaStore (Android 10+), sin permisos de archivos.
- **Solo ilustración:** los efectos no demuestran efectos cuánticos, ruptura física de realidad o capacidad de REVERSE-PC.

## Compilar

```sh
gradle -p space-glitch-android --no-daemon :app:assembleDebug
```

APK: `space-glitch-android/app/build/outputs/apk/debug/app-debug.apk`.

Paquete `com.reversepc.spaceglitch`, independiente de Mundo X Fusion3D y de REVERSE-PC. Android 10+ / OpenGL ES 3.0. El flujo `.github/workflows/space-glitch-v2.yml` genera el APK, inicia un emulador Android y guarda capturas y evidencia.
