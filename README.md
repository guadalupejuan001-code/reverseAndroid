# reverseAndroid

Repositorio propio del emulador Android de REVERSE, basado en el modelo compacto XI/Ether.

La migración se realiza desde `guadalupejuan001-code/REVERSE-PC`, rama `fix/android-c68-saturating-narrow-compact`, commit `37f5b4610c5c31981e359fcf8fd0b303c14f01fd`.

Última frontera independiente de C68: **64.295.950.532 instrucciones**, checkpoint SHA-256 `f306e9ef430186b8839a75a4e29c4f57cda106f9b958ab0c5db10670c246b18e`.

La ejecución conserva el formato compacto, el decodificador original, XI/Ether, 2 GiB de RAM lógica y los controles originales. `system_server`, `sys.boot_completed=1` y contenido Android visible siguen pendientes. Exportar una imagen negra no acredita el arranque completo.

En curso: importar el código y sus pruebas, autenticar y preservar la frontera C68, y continuar la ejecución desde este repositorio.
