# Continuar un presupuesto Android verificado

El arranque completo y la pantalla todavía requieren evidencia real. C25 ya ejecutó las instrucciones UADDW y ST1 que bloqueaban a `aconfigd`; una prueba local avanzó 53.000.000 instrucciones y terminó limpiamente por presupuesto del host. La prueba larga de C25 continúa por separado.

`android_profile_resume.mjs` permite continuar estados de este perfil sin recompilar constantes para cada presupuesto. Exige un contrato con las cuatro huellas SHA-256 exactas de `current.rvg`, `current.json`, `result.json` y `process-outcome.json`. Comprueba el formato binario, la identidad de kernel/initrd/imagen/disco, el perfil de 2 GiB, la semilla/cadencia/fase del reloj y el informe. Rechaza errores arquitectónicos, pánicos, reinicios, estados detenidos, cambios de perfil y atribuciones falsas de entrada a `/init`.

El binario vuelve a comprobar las huellas del estado y su informe, la identidad completa del disco y el punto PC/instrucciones/fase. La restauración debe volver a producir exactamente los bytes de entrada. El límite explícito de transporte de 1 GiB sigue fuera del RVG; no se cambian CPU, RAM, disco ni reloj. El diario guarda generaciones completas y solo continúa presupuestos limpios. La captura de pantalla se produce únicamente si el dispositivo gráfico emite una imagen.

Preparar el código con las fuentes fijadas del proyecto:

```sh
bash scripts/setup-ether.sh
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
python3 scripts/build-softfloat.py --out build/softfloat
g++ -std=c++17 -O2 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1 \
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools \
  tools/guest_profile_resume.cpp build/softfloat/libreverse_softfloat.a \
  -o build/profile-native
```

Colocar las cuatro evidencias originales en `input/`. Verificar el disco reconstruido con el paquete de recuperación; debe medir 17.330.864.128 bytes y tener SHA-256 `0e829fad2cd1cfef8ea707044270d97559127c6cd48bdf268e7eb5cf1b573ce6`. No usar el estado terminal C19.

```sh
node tools/android_profile_resume.mjs preflight resume-contract.json input
node tools/android_profile_resume.mjs run resume-contract.json input \
  build/profile-native android-c20.raw new-output 900000 2000000000 3
```

Los tres últimos argumentos son milisegundos por presupuesto, límite de instrucciones y número de presupuestos (1 a 4). `new-output` debe ser nuevo. El supervisor publica el resultado real y devuelve un código de error ante un fallo; nunca convierte ausencia de errores en prueba del framework. Las instrucciones y pruebas de entrada anteriores se conservan como procedencia, sin atribuirlas a una nueva ejecución.

Para observar una generación mientras el proceso sigue activo, `android_pin_checkpoint.mjs` fija sus bytes mediante un enlace de archivo, valida su informe y perfil y publica un directorio independiente. La limpieza del diario no elimina la observación fijada. Es una observación; no equivale a un presupuesto terminado ni prueba `boot_completed`.

```sh
node tools/android_pin_checkpoint.mjs new-output/journal observed-prefix \
  SHA256_ORIGEN profile-budget-continuation COMMIT_DE_EJECUCION INSTRUCCION_MINIMA
```

El contrato del punto C25 local está en `tools/android_c25_local_contract.json`. Ese contrato identifica únicamente el punto de 4.611.188.058 instrucciones; no debe reutilizarse para otro resultado de CI. Las comprobaciones de host incluyen 37 pruebas negativas/de límites y una observación real que conserva exactamente el mismo archivo del diario.
