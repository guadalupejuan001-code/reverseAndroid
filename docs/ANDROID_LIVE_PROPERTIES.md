# Propiedades observadas en el espacio de PID 1

El observador admite `--live-properties` después del límite de transporte. Obtiene los tipos y offsets del BTF del kernel cuya huella completa ya ha sido comprobada. Recorre el árbol Maple de `mm_struct`, valida padres, límites, VMAs y su número, resuelve las rutas a través de dentries y montajes y selecciona las áreas compartidas de propiedades de PID 1. Lee sus páginas mediante el PGD de ese proceso; comprueba permisos de usuario y no supone adyacencia física.

```sh
node tools/android_kernel_observer.mjs checkpoint.rvg SHA256 1073741824 --live-properties
```

La propiedad se acepta únicamente con el formato Bionic completo, trie válido, nombre exacto, serial estable y valor estructurado. Las áreas que superan una página se leen a través de sus traducciones virtuales. Los valores contradictorios se marcan ambiguos. Una estructura transitoria, página no presente o ruta no verificable genera un diagnóstico; no se convierte en éxito.

En el punto C25 local de 4.611.188.058 instrucciones se validaron las 1.099 VMAs de PID 1 y 393 áreas de propiedades. `sys.boot_completed` todavía no estaba presente. Las 43 pruebas incluyen árboles inválidos, páginas físicamente separadas, permisos jerárquicos, propiedades posteriores a la primera página y corrupción de bits altos. El resultado por defecto del observador conserva exactamente los bytes del informe anterior sobre el C23 auténtico.

También se corrigió un rechazo falso del analizador: el nombre vacío del nodo raíz de Bionic comparte dirección con su zona de respaldo de valores modificados. No debe exigirse un terminador cero allí. Los tokens no vacíos sí mantienen ese requisito. El respaldo de dos áreas `init.svc` del C25 real contenía valores anteriores válidos; ahora se inspeccionan correctamente.

Todo el análisis es de lectura. `live_property_mapping_verified` y `boot_completed_property_live` distinguen la propiedad encontrada en una asignación real de PID 1 de una cadena suelta en RAM. El observador mantiene `android_framework_verified` y `android_display_verified` en falso: completar esas pruebas requiere corroborar la ejecución del framework y revisar la imagen realmente emitida.

Referencias de formato: [Linux Maple Tree v6.12](https://github.com/torvalds/linux/blob/v6.12/include/linux/maple_tree.h), [reglas de nodos](https://github.com/torvalds/linux/blob/v6.12/lib/maple_tree.c) y [área de propiedades de Bionic](https://github.com/aosp-mirror/platform_bionic/blob/main/libc/system_properties/include/system_properties/prop_area.h).
