# W4u 0.1 dentro de BBY Q y REVERSE-PC

**W4u (work for u) es el motor de trabajo. BBY Q es la cara conversacional.**
REVERSE-PC aloja sus ventanas, programas y ejecución nativa. El motor se
implementa en `BBY-Q/ai/xi/w4u`; esta entrega enlaza esos módulos originales en
BBY Q y entrega también `W4u.rxe`, un componente de objetivo tipado reutilizable.

## Uso

En BBY Q, dentro de REVERSE Control/Workbench:

| Petición | Plan |
|---|---|
| `maximiza calculadora` | Abrir si falta; después maximizar |
| `ejecuta programa 1` | Abrir si falta; después ejecutar el RXE interno |
| `run program 1` | El mismo plan, con respuesta inglesa si está seleccionada |
| `abre programa 1` | Abrir la ventana sin ejecutar su código |
| `muestra archivos` | Abrir si falta; después enfocar |

El número de cada programa aparece en Aplicaciones. Se exponen los primeros
16 RXE del catálogo, ordenados por ruta, excluyendo BBY Q. El número puede
cambiar al modificar el catálogo; cada observación vincula número, ruta e
identidad exacta. La interfaz vuelve a consultar el catálogo al enviar una
petición. Los chats pueden abrirse, pero no tienen capacidad `runApp`.

Los programas se ejecutan con `NativeRuntime.executeRxe` y el `SessionStore`
del escritorio, dentro de REVERSE. Su salida aparece en su ventana. La memoria
conversacional privada de BBY Q/W4u permanece separada del estado anfitrión.
No se delega el razonamiento ni la ejecución a un servicio de IA externo.

## Planificación y comprobación

Tres roles cooperativos en XI comparten la memoria privada RVPC2:
planificador, explorador y verificador. Son módulos simbólicos secuenciales con
registros propios; esta versión propone una ruta directa o una con un
prerrequisito, de hasta dos pasos. La gramática de órdenes está diseñada y es
finita; no se afirma que exista planificación general ni capacidad comparable
a Astra. El checkpoint aprendido E022 no se reentrena.

El verificador comprueba la relación del plan con el objetivo y las capacidades
antes de cada paso. Después de actuar, el anfitrión entrega una observación
nueva: una apertura que no produjo una ventana detiene la continuación aunque
el anfitrión hubiera indicado éxito. Para ejecutar un RXE se exige que su
ventana esté abierta y que conserve capacidad de ejecución.
Los resultados de ejecución RXE, ordenar y «esta ventana» dependen del resultado
tipado del anfitrión, sin una inspección visual o semántica independiente.

Java verifica las identidades y metadatos contra archivos RXE nativos reales.
JavaScript aplica únicamente operaciones tipadas. Cada paso usa un ticket
nuevo, vinculado al catálogo y a la identidad/espacio/visibilidad de las ventanas.
Un cambio de geometría por el teclado no cambia esa identidad. Una vista
cambiada, ticket repetido o capacidad ausente detiene el paso.

Solo la respuesta a una petición nueva o una confirmación puede devolver una
acción. Abrir una ventana, reconstruir el catálogo o reiniciar nunca devuelve
un plan para ejecutar. Un programa ya ejecutado no se repite automáticamente
si se pierde la confirmación posterior. Una orden nueva sustituye el episodio
pendiente; no reanuda acciones anteriores.

## Actualización y paquete Android

El constructor genera migraciones XI con comprobación byte por byte desde el
E016 conocido y desde Control-1. Verifica que núcleo y enlaces coincidan,
conserva hechos/cuentas/idioma/historial fuera de la diferencia y rechaza
conflictos. Solo reconoce las identidades exactas archivadas; un ejecutable
personalizado no se sustituye. Se conserva la sesión anterior como respaldo.

La APK local **REVERSE Control 2.3.0**, paquete
`com.reversepc.control.workbench`, usa la misma clave de desarrollo que Control
2.2.0 y puede actualizar esa edición. Es una instalación separada del paquete
Workbench principal; no puede leer sus datos privados. Java, recursos, interfaz
y RXE se recompilan. Las bibliotecas JNI ARM64/x86_64 se conservan sin cambios
desde la APK anterior del propio proyecto, cuya firma se comprueba.

## Validación local

- Motor aislado W4u: **21 comprobaciones**, ejecutando XI real.
- Integración nativa W4u: **37 comprobaciones**, incluyendo planes de dos
  pasos, una confirmación falsa, capacidades retiradas, propuesta corrupta,
  equivalencia del componente `W4u.rxe`, y conocimiento conservado al migrar.
- Escritorio: **83 comprobaciones**, **19 acciones con XI real** y un RXE de
  prueba que incrementa un byte del estado nativo del escritorio. Se confirma
  que su ventana existe y que el contador realmente cambió.
- Regresión de gramática/conversación/migración: **1.127 comprobaciones**;
  evidencia en `docs/evidence/w4u-control-native.json`.
- Clases Java compiladas contra Android 35; APK firmada, DEX, ambas ABI y
  alineación verificadas durante el empaquetado.

El dibujo DOM y almacenamiento Android de las pruebas de escritorio son
fixtures. No se ha lanzado esta APK en un dispositivo/emulador durante esta
validación. Persisten el límite de 255 bytes/15 palabras y los planes de dos
pasos; no hay conversación o control visual general.

## Reproducir

```bash
# Fuentes BBY-Q con W4u: 86b52437d851c7c6db8a80a1d2d4c3e5e2affb09
node bbyq-interface/build.mjs /ruta/reverse_pc /ruta/BBY-Q /ruta/build /ruta/BBY-Q-E016.rxe --install-assets
node /ruta/BBY-Q/ai/xi/w4u/tests/native-w4u.mjs /ruta/reverse_pc /ruta/w4u-core
node bbyq-interface/tests/w4u-native.mjs /ruta/reverse_pc /ruta/build /ruta/BBY-Q
node bbyq-interface/tests/native-control.mjs /ruta/reverse_pc /ruta/build
node bbyq-interface/tests/desktop-control.cjs /ruta/reverse_pc /ruta/build
```

La CI queda preparada para reconstruir y ejecutar estos exámenes. Su referencia
BBY-Q debe publicarse antes de ejecutar la integración desde GitHub. Los
resultados descritos arriba son locales; no se presentan como una CI aprobada.
