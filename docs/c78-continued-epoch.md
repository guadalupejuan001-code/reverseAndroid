# Continuación C78 de la misma época Android

C78 parte del último checkpoint completo de C77, etapa 16, a **23.000.000.000 instrucciones**: `e6beaed954011ff958b2bf9c9a4b4463bc3ee7b9fea2560e6a812d20497926fc`. Se recuperaron y autenticaron las tres piezas, los ZIP, el snapshot original completo y la admisión original de la época. [Recuperación](evidence/c77/stage16-original-recovery.json).

La ejecución mantiene la CPU, RAM, disco efectivo, semilla, 64 instrucciones por tick, comprobaciones de IRQ, formatos y decodificadores originales. No declara otra época ni introduce entrada UART. La revisión de hilos a 23 mil millones encontró un único hilo de Zygote, con nice −20, ejecutando `LZ4_decompress_safe` en el runtime de ART. [Observación de solo lectura](evidence/c77/stage16-threads-readonly.json).

El flujo conserva hasta 40 tramos acotados de mil millones de instrucciones. Cada resultado debe pasar las comprobaciones originales y guardar todas sus piezas antes del siguiente. Cada cuatro etapas se añaden observaciones de solo lectura de luces y de los hilos reales de Java; esas observaciones no modifican la máquina ni sustituyen sus condiciones de aceptación.

C77 completó y conservó correctamente sus 16 tramos. Falló únicamente su comprobación final de arranque Android: `system_server`, el `sys.boot_completed=1` vivo, la interfaz y la respuesta a una entrada siguen pendientes. C78 mantiene esa misma comprobación; al obtener un frame y el arranque real, aún se inspeccionará la interfaz y se probará una entrada auténtica.
