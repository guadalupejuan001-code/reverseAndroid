# C23: continuación exacta de C22 y transporte explícito

C22 ejecutó 2.511.100.000 instrucciones adicionales y terminó limpiamente por
presupuesto de tiempo en 4.485.354.074 instrucciones. El bootstrap de APEX
activó 87 paquetes y finalizó; init comenzó a leer sus servicios. No hay todavía
framework ni pantalla de Android verificados.

El checkpoint C22 ocupa 261.353.084 bytes: quedan 7.082.372 bytes bajo el límite
anterior de 256 MiB. C23 permite elegir un presupuesto de transporte de hasta
1 GiB para esta continuación adaptativa. El valor predeterminado sigue siendo
256 MiB y el transporte histórico de huéspedes con límite de overrides sigue
en 48 MiB. El presupuesto es finito y no representa RAM física adicional.

Esta política del anfitrión no forma parte del RVG. Aumentarla no cambia
registros, memoria, dispositivos, semilla, frecuencia, fase del reloj, kernel,
initrd ni disco. El flujo exige restauración byte a byte del C22 original antes
de ejecutar. No se vuelve a arrancar desde cero y no se sustituyen XI/Ether,
AVB ni SELinux.

La prueba con UBSan compara las mismas instrucciones, bytes y dispositivos con
política predeterminada y explícita, comprueba el límite exacto y el rechazo de
políticas fuera de rango, y confirma que un fallo al serializar no modifica la
CPU. La política no se hereda al restaurar un RVG sin selección explícita.
El auditor independiente mantiene también un límite explícito y finito.

Antes de abrir el disco, el binario informa sus huellas compiladas y el auditor
independiente comprueba que coincidan con las del estado, informe, disco y
semilla originales. Esto detecta una divergencia entre verificadores antes de
restaurar o ejecutar el invitado.

La salida publica un checkpoint canónico. Si la auditoría no puede publicarlo,
se preserva por separado el último prefijo atómico para diagnosticarlo; no se
presenta automáticamente como un estado sano. Se omiten copias redundantes del
diario en la transferencia, conservando el estado real y la evidencia.

El disco original y el punto C21 también están preservados en un
[borrador de recuperación](https://github.com/guadalupejuan001-code/REVERSE-PC/releases/tag/untagged-94994ad8d24b73285a61),
con las seis huellas de archivos verificadas contra los activos guardados.
