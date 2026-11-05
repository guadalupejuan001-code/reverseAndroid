# Android C37 · caché coherente de lectura de instrucciones ARM64

## Problema observado
El invitado Android C35 permanece dentro del servicio KeyMint mientras genera/usa material criptográfico. Su hilo principal TID 409 (prioridad normal, `fair_sched_class`) está ejecutando `/apex/com.android.hardware.keymint/lib64/libcrypto.so` a una velocidad de emulación que hace costosa su continuación.

## Cambio de ejecución real XI/Ether
Una caché *solo de instrucciones*, en el intérprete ARM64 original, puede evitar cuatro accesos repetidos a los mismos bytes del Ether por cada instrucción caliente. No sustituye memoria ni simula resultados de criptografía.

- Cada búsqueda comprueba de nuevo la traducción y los permisos ejecutables de MMU, sin saltar el control de EL0.
- La clave de la caché es la dirección **física** validada de la instrucción (no la dirección virtual), usando 8192 entradas de reemplazo directo.
- Escrituras de CPU y pares atómicos invalidan la palabra física afectada; DMA invalida las palabras afectadas. Escrituras externas XI/RXE y carga de otra imagen invalidan globalmente.
- Si existen ligaduras cuánticas que puedan acoplar otros bytes, se omite la caché de lectura y se invalida al escribir; no se presume independencia.
- La caché no está en snapshots RVG ni cambia el reloj virtual, GKI, disco, AVB, SELinux, Binder, KeyMint o el formato de memoria del invitado.
- `REVERSE_PC_INSTR_CACHE_REFERENCE` permite compilar la misma CPU en modo lectura original.

## Prueba diferencial exigida
La CI crea **dos intérpretes** desde el mismo commit: referencia sin caché y caché habilitada. Cada uno reanuda desde el checkpoint C35 de 15,317,348,839 instrucciones y ejecuta exactamente **5,000,000** instrucciones KeyMint reales desde el mismo estado y el disco C27 original.

Se exige identidad **byte por byte** del nuevo snapshot RVG, del PC, contador, memoria Ether, estado de CPU y reloj. Solo después se puede considerar aprobada la optimización. Se informan los tiempos del host de ambas ejecuciones, sin confundirlos con FPS Android.

Si la prueba falla, no se utilizará la rama como nueva fuente. Tampoco se declarará Zygote hasta observar el proceso real y comprobar las propiedades de inicio.

## Fuentes
- C35 genuino: https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41347214737
- C36 KeyMint continuación: https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41352354242
