# Android: C21 verificado y continuación C22

Estado comprobado: el kernel y `/init` originales ejecutan en el mismo
XI/Ether de REVERSE-PC; segunda etapa de init y SELinux enforcing observados.
El framework, la pantalla de Android y Wild Rift siguen sin verificarse.

## Evidencia actual

| Punto | Resultado real | Reanudación |
| --- | --- | --- |
| C19 | APEX gatekeeper ambiguo, SIGABRT y reinicio solicitado | No reanudar esa CPU |
| C21 | Arranque nuevo con perfil HAL explícito; 1.974.254.074 instrucciones; límite limpio de tiempo | Sí, conservar CPU, RAM, disco y reloj |
| C22 | Continuación exacta de C21 en tres presupuestos consecutivos | Consultar el artefacto de la ejecución; el proceso exitoso no prueba Android completo |

C21: [ejecución 41102829244](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41102829244),
artefacto `reverse-c21-fresh-apex-android`.
C22: [ejecución 41112055019](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41112055019).

El estado C21 SHA-256 es
`8c26488967ee3a9b2a55313810e92bce5ee2a182dbe81e8374bd2f6d3007ea05`.
Su perfil declara 2 GiB; conserva el kernel y los archivos CPIO originales.
El trailer bootconfig agrega selecciones HAL existentes del perfil oficial
Cuttlefish: keymint/gatekeeper locales y gráficos por software. AVB y SELinux
siguen activos. No se carga otro VMM.

El disco extendido agrega userdata oficial del mismo build y misc. Las ocho
particiones originales conservan posición y bytes. El SHA-256 del disco nuevo
es `0e829fad2cd1cfef8ea707044270d97559127c6cd48bdf268e7eb5cf1b573ce6`;
no se puede sustituir el disco dentro de una CPU de un perfil anterior.

## Continuar y comprobar

El flujo `.github/workflows/android-c22-continuation.yml` reconstruye y verifica
el disco, comprueba las identidades del estado y sus informes, compila el mismo
runtime y exige restauración byte a byte antes de ejecutar. Los límites de
15 minutos sólo dividen la planificación del anfitrión: no reinician la CPU.
Un fallo ISA, panic o reinicio del invitado termina la continuación.

`tools/android_c22_result.mjs` separa los avances de ejecución, los fallos del
invitado y las pruebas de arranque. La evidencia de `/init` pertenece a C21;
C22 no la presenta como una nueva entrada observada.

`tools/android_boot_property.mjs` inspecciona la propiedad Bionic
`sys.boot_completed` en páginas RAM reconstruidas sin alterar el estado.
Valida cabecera, trie, nombre, serial y valor; una cadena suelta no es prueba.
La observación RAM por sí sola tampoco verifica que la página siga mapeada ni
que el framework o la pantalla estén listos. Si un dato cruza la primera página,
se exige un análisis del mapa virtual en vez de asumir contigüidad física.

Las pruebas independientes del lector cubren actualizaciones incompletas,
offsets corruptos, ciclos, orden de búsqueda Bionic, deltas XOR, integridad del
snapshot y direcciones externas de 64 bits. No son una simulación del arranque.

El inspector del kernel también admite puntos posteriores a la liberación de
datos de init: informa los símbolos que ya no están mapeados sin descartar el
resto de la evidencia. En EL0 resuelve el hilo actual mediante `__entry_task`
y el offset por CPU, y valida su pertenencia a la lista de hilos del proceso.
Distingue los registros vivos del contexto de kernel guardado y no presenta
la lista de líderes como un inventario completo de todos los hilos.
En el punto C21 identifica al hilo `ueventd` PID 92 sin modificar el estado.

## Preparación del disco

La copia del disco conserva huecos vacíos cuando el sistema de archivos no
permite reflink. Evita convertir innecesariamente el disco disperso en una
copia física de 8,7 GB. La reconstrucción real con esta ruta produce el mismo
SHA-256 `0e829fad2cd1cfef8ea707044270d97559127c6cd48bdf268e7eb5cf1b573ce6`
y comprueba todas las particiones originales. La longitud lógica del disco y
los bytes que ve Android son idénticos.

## Referencias del perfil y formato

- [Cuttlefish bootconfig oficial, revisión fijada](https://github.com/google/android-cuttlefish/blob/c857a805bd20fe32a791eb3552ed0c43d48b6452/base/cvd/cuttlefish/host/commands/assemble_cvd/bootconfig_args.cpp).
- [Formato de propiedades Bionic](https://github.com/aosp-mirror/platform_bionic/blob/main/libc/system_properties/include/system_properties/prop_area.h).
- [Valor y serial Bionic](https://github.com/aosp-mirror/platform_bionic/blob/main/libc/system_properties/include/system_properties/prop_info.h).
