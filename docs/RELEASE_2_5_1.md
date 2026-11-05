# REVERSE 2.5.1 — integración de las entregas pendientes

Integra la historia conservada de REVERSE 2.2–2.4.2, el agente BBY Q
`E025-Chain-1` recuperado de la APK 2.5.0 y el motor W4u 0.1 original.
La edición Control mantiene el paquete `com.reversepc.control.workbench`.

La APK 2.5.0 había perdido los cambios del teclado entregados en 2.4.2.
2.5.1 conserva la conexión del cuadro de texto, el borrador, la selección,
el botón de ocultar teclado y la reserva del IME en el escritorio horizontal.
No añade un modelo de IA externo ni cambia los módulos de W4u.

## Procedencia recuperada

REVERSE hasta `476011fabff503c04957d88367805313361a4138` y BBY-Q hasta
`86b52437d851c7c6db8a80a1d2d4c3e5e2affb09` se recuperaron de sus bundles.
El programa E025, su estado, currículo y migraciones se recuperaron byte a
byte de la APK 2.5.0. Los commits E025 originales citados en ella no estaban
disponibles. La carpeta `ai/releases/e025` de BBY-Q conserva y explica esa
recuperación; recompila su XI y comprueba la equivalencia de cada instrucción.

El código Java de las migraciones E025 se reconstruye a partir del anfitrión
incluido en esa APK y se integra con las fuentes de teclado conservadas.
Un conflicto retiene el ejecutable y estado anteriores. Una acción pendiente
pospone la migración; el estado de destino no puede sustituir otra sesión.

El checkout de mundo-X permanece fijado en
`e2778d962af22f6f1a0794058dd4685fc2573966` sin modificaciones. Una copia local
verificada de `inverse_memory.hpp` amplía de 65.536 a 524.288 el presupuesto de
overrides; el formato RVPC2 se conserva.

## Compilación

La CI fija el commit de BBY-Q, construye el runtime y verifica E025/W4u,
las herramientas cuánticas, el escritorio y el teclado. Después genera las
APK ARM64/x86_64 e instrumentación Android 35 y comprueba el recorrido real
en emulador. La APK de distribución se vuelve a firmar localmente con la
clave de actualización conservada. La clave no se publica en GitHub.

El manifiesto de distribución identifica el commit compilado, hashes de
APK/RXE y el certificado. Una prueba en emulador no garantiza el comportamiento
del teclado concreto de cada teléfono.
