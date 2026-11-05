# Compatibilidad ARM64 en el ejecutor existente de REVERSE

## Alcance real

La función se abre en **XI → ARM64**, dentro del mismo escritorio REVERSE.
No tiene un programa Guest independiente ni otra implementación de motor cuántico.
Los accesos a memoria usan `QuantumRuntime`/`InfiniteCore`/`InverseMemory` originales.
El módulo conserva una sesión de máquina propia, como los experimentos aislados:
no mezcla direcciones, puertas ni checkpoints con el trabajo del escritorio o BBY Q.

El decodificador ARM64 y el control clásico del procesador están escritos en C++.
Es emulación de instrucciones y de algunos dispositivos; **no es una traducción
ARM64→XI, una ejecución de Android en qubits, ni un acelerador cuántico del juego**.
XI/RXE transforman la autoridad de memoria que utiliza el módulo. MX, XASM, QX y
MXVM no adquieren compatibilidad automática con aplicaciones Android.

**Android completo y Wild Rift todavía no arrancan.** Esta entrega ejecuta
programas estáticos sencillos y un testigo de sistema propio. El cargador acepta
el protocolo de una Linux `Image` acotada, pero eso no demuestra un arranque Linux.
La CI compila Linux 6.1 y registra dónde se detiene el intérprete propio.

## Una base, reglas y diferencias exactas

`byte físico = imagen inmutable XOR delta XI/Ether`

- ELF o contenedor `RVI1` de Image/DTB/initrd se conserva como respaldo inmutable.
- RAM no modificada/BSS se describe con fondo cero. No se reserva un arreglo
  proporcional al espacio lógico declarado.
- Un store ARM64 usa una transacción de bytes tipados del mismo `QuantumRuntime`.
  Un bloque mantiene un candidato común para núcleo y memoria; evita volver a
  analizar texto XI y copiar metadatos por cada palabra. Las operaciones siguen
  llamando al `InverseMemory::store` original, con sus revisiones y enlaces cuánticos.
  Restaurar el byte de respaldo elimina la diferencia redundante. Escribir cero
  sobre una imagen no cero sí necesita una diferencia.
- XI/RXE pueden alterar ese delta y el núcleo cuántico original. Leer o escribir
  un byte cuántico no determinista falla sin medirlo ni borrar su estado.
- Registros, tablas escritas por el programa, bancos SP, excepciones, temporizador,
  control del GIC y cola serial son información necesaria y se conservan.
- Las copias candidatas comparten respaldo/shards sin cambios. Serializar un
  checkpoint sí crea temporales acotados; también tienen coste físico.

«Estados mínimos» tiene este alcance concreto. No se afirma una representación
matemáticamente mínima de todos los programas ni compresión universal. Muchas
escrituras distintas pueden hacer costoso el mapa de excepciones por byte. Una
memoria adaptativa por páginas sigue pendiente; ahora hay perfiles del arranque
parcial Linux y comparaciones de transacciones de bytes. Estos límites deben resolverse antes de pretender ejecutar un Android con datos de juego.

Una prueba prepara **4.096 qubits independientes** con XI y luego ejecuta el testigo
ARM64 sobre la misma autoridad. El snapshot/revisión del núcleo cuántico permanece
idéntico. Esto preserva la capacidad existente, sin afirmar simulación arbitraria
entre todos esos qubits ni mejora de FPS.

## Capacidades implementadas

| Parte | Alcance |
| --- | --- |
| Aplicación | ELF64 little-endian AArch64 ET_EXEC estático; shim `write`/`exit`/`exit_group` |
| Sistema | Un núcleo AArch64 EL1/EL0, bancos SP, DAIF/NZCV, MRS/MSR seleccionados, SVC/ERET |
| Excepciones | Síncronas/IRQ, VBAR/ELR/SPSR/ESR/FAR; permisos y fallos de traducción |
| MMU | Stage 1, TTBR0/TTBR1, 25–48 bits VA, granulo 4 KiB, bloques L1/L2 y páginas L3; AF/AP/APTable/PXN/UXN y prohibición de ejecutar en EL1 regiones escribibles por EL0 |
| Consola | Subconjunto PL011 con entrada/salida y cola acotada |
| Interrupciones | Subconjunto GICv2 de un grupo y un núcleo, 64 IDs; inicialización APR en cero sin IRQ activa; temporizadores físico/virtual |
| Tiempo | Un tick virtual por transición de ejecución; WFI puede avanzar hasta el siguiente plazo; no es reloj de pared |
| Arranque Image | Image AArch64 little-endian de 4 KiB, DTB v17 generado y opcional initrd |
| PSCI | VERSION, FEATURES seleccionadas, SYSTEM_OFF; otras llamadas devuelven NOT_SUPPORTED |
| FP/SIMD | Subconjunto escalar FP32/FP64 IEEE 754, conversiones FP16, registros V0–V31, memoria vectorial y operaciones NEON seleccionadas; FPCR/FPSR y trap FPEN |
| Checkpoint | RVG1/RVG2 conservan sus bytes cuando FP está en cero; RVG3 añade únicamente el estado FP/SIMD no nulo |

El subconjunto entero incluye las variantes implementadas en el decoder para
MOVN/Z/K, ADD/SUB inmediatos/desplazados/extendidos, ADC/SBC, lógica inmediata y con
registro, bitfields, EXTR, desplazamientos variables, CSEL, CCMP, RBIT/REV/CLZ/CLS, divisiones,
MADD/MSUB, ramas, cargas/stores enteros comunes y pares, incluido LDPSW con extensión de signo. Los exclusivos simples
LDXR/STXR se habilitan en modo sistema; LDAR/STLR usan memoria coherente síncrona.
El monitor exclusivo se invalida con escrituras/operaciones XI/RXE/excepciones y
se conserva en RVG2. No hay cobertura completa del ISA.

La aritmética FP usa Berkeley SoftFloat 3e fijado por hash, con configuración
ARM-VFPv2. El núcleo cuántico y la autoridad de memoria se mantienen. El fondo
de los 32 registros vectoriales es cero; RVG3 usa una máscara y 16 bytes por
registro no nulo: entre 20 y 532 bytes adicionales, más los campos de longitud versionados.
FPCR/FPSR también se conservan. No se afirma cobertura completa de NEON: faltan,
entre otras, operaciones saturadas, reducciones y desplazamientos seleccionados,
criptografía y extensiones posteriores de Arm. No se implementan traps de
excepciones aritméticas FP; los bits de habilitación no se anuncian. AHP no se
admite. Los casos no soportados fallan de forma explícita.

No se implementan EL2/EL3, virtualización KVM, multicore, enlazado ELF
dinámico, arranque Android, Binder/ART/Bionic, almacenamiento/virtio, GPU de
invitado, audio o red de invitado. Registros/instrucciones/dispositivos no soportados
producen errores explícitos. Mantenimiento de caché seleccionado actúa sobre una
máquina coherente sin caché/TLB arquitectónico modelado; no se simula su latencia.

Un error de implementación no soportada descarta el bloque candidato completo.
Un fallo arquitectónico implementado entra en el vector del invitado y permite
que su manejador continúe. Un bloque con presupuesto agotado conserva el avance.

## Probar desde REVERSE

1. Abre **XI → ARM64**.
2. **Comprobar modo de sistema** prepara el testigo propio; no es Android/Linux.
3. **Ejecutar bloque** debe producir `REVERSE: EL1 MMU IRQ EL0 SVC FAULT OK`.
4. **Comprobar FP/SIMD**, seguido de **Ejecutar bloque**, verifica suma FP32,
   raíz FP64 y registros/memoria NEON; imprime `REVERSE: FP32 FP64 SIMD OK`.
   También puede ejecutarse con checkpoints entre bloques.
5. Los botones permiten importar ELF de aplicación, ELF de sistema o Linux Image.
   La imagen debe encajar en los límites y usar instrucciones/dispositivos soportados.
6. En modo sistema, **Consola → Enviar** agrega entrada a PL011. Ejecuta otro bloque
   para que el programa la procese. Cerrar/reabrir conserva la sesión de máquina.
7. **XI sobre el delta de memoria** modifica esa misma memoria. Una dirección sin
   respaldo tiene byte=delta; una dirección dentro de imagen combina por XOR.

El testigo monta tablas reales, activa la MMU, programa un timer/GIC, entra por
IRQ y retorna, pasa a EL0, ejecuta SVC y provoca una escritura denegada. Solo
imprime éxito después de verificar esos efectos. QEMU se usa únicamente en CI
para contrastar el ELF ensamblado independientemente; no se incluye en REVERSE.

El flavor Android de integración es el **REVERSE existente**, paquete
`com.reversepc.control.workbench`, versión de código 253. La compilación CI usa una
clave desechable de pruebas: no es una actualización con firma verificada para
el APK ya instalado por el usuario. No se pide desinstalar ni perder sus datos.

## Límites

| Recurso | Presupuesto |
| --- | --- |
| RAM lógica EL0 | 64 KiB–256 TiB; no representa RAM física disponible |
| RAM lógica de sistema | 64 MiB hasta rango físico de 48 bits; UI usa 128 MiB |
| RAM física de sistema | Comienza en `0x40000000` |
| Imagen ELF / Linux | ELF: 16 MiB (sin cambios); Linux ARM64 Image: 64 MiB kernel, 64 MiB initrd y 128 MiB de contenedor total (nuevo perfil CI); ELF hasta 64 PT_LOAD |
| Excepciones de byte | 1–1.048.576; Android 65.536 |
| Snapshot CI/CLI / transporte Android | RVG convencional 48 MiB; RVG adaptable hasta 256 MiB bajo perfil explícito; Android UI 32 MiB y exportador 8 MiB aún no adaptados |
| Bloque CLI / Android | 1.000.000 / 10.000 transiciones máximo |
| Salida por bloque / entrada serial | 65.536 / 4.096 bytes |

El checksum FNV detecta corrupción accidental y no autentica snapshots. RVPC2 y
RXE mantienen sus formatos y rutas actuales. La UI no tiene importación RVG;
la restauración automática se realiza desde su archivo privado conservado.

## CLI y validación

```sh
bash scripts/setup-ether.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target reverse_guest reverse_pc_minimal_guest reverse_pc_minimal_system reverse_pc_minimal_fp reverse_pc_minimal_bytes --parallel 2
build/reverse_pc_minimal_guest
build/reverse_pc_minimal_system
build/reverse_pc_minimal_fp
build/reverse_pc_minimal_bytes
build/reverse_guest fp-demo
build/reverse_guest system-demo-init inicial.rvg
build/reverse_guest run inicial.rvg parcial.rvg 30
build/reverse_guest run parcial.rvg final.rvg 1000
build/reverse_guest peek final.rvg 1073778696 8
build/reverse_guest system-init propio.elf propio.rvg
build/reverse_guest linux-init Image linux.rvg initrd.cpio
build/reverse_guest device-tree reverse.dtb
build/reverse_guest input linux.rvg entrada.txt con-entrada.rvg
build/reverse_guest xi final.rvg cambios.xi cambiado.rvg
build/reverse_guest rxe cambiado.rvg cambios.rxe continuado.rvg
```

El testigo de sistema conserva 128 MiB lógicos con una imagen de 4.391 bytes y
9 diferencias de byte al terminar. Es un test pequeño, no la huella de Android
ni de Wild Rift. Las pruebas originales aportan 8.234 aserciones; las nuevas
comprueban 58 condiciones de sistema/tabla/permisos/estado cuántico, además de
CLI, UBSan y oráculos clang/lld/QEMU. La integración Android ejercita WebView/JNI,
el ejecutor original y reinicio en emulador Android 15. No equivale a una prueba
física en el teléfono.

La prueba `examples/guest-fp-simd.S` compara resultados binarios, FPSR, NaN,
ceros con signo, redondeos, FZ/DN, conversiones y vectores con clang/lld/QEMU.
También exige igualdad completa entre ejecución continua y checkpoints cada
137 instrucciones. Las pruebas nativas comprueban aislamiento de redondeos entre
hilos, estado disperso, compatibilidad RVG1/RVG2 y rollback.

La transacción de bytes tiene 213 comprobaciones, incluyendo escrituras
cuánticas deterministas, lectura del candidato actual, rollback de ambos estados,
límites y 1.700 stores aleatorios contrastados con XI. Un benchmark separado
mide 12.000 stores; no predice FPS. Una variante de prueba traduce cada byte por
separado y debe producir exactamente el mismo checkpoint Linux de 500.000
instrucciones que la ruta que reutiliza el mapeo dentro de un acceso. No existe
un TLB persistente: cada acceso observa las tablas actuales. Se prueban páginas
no adyacentes, permisos y fallos sin stores parciales.

Referencias primarias: [arranque Linux AArch64](https://docs.kernel.org/arch/arm64/booting.html),
[Linux 6.1 fijado](https://github.com/torvalds/linux/tree/830b3c68c1fb1e9176028d02ef86f3cf76aa2476),
[ABI ELF AArch64](https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst),
[arquitectura de memoria Arm](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/LearnTheArchitecture-MemoryManagement-101811_0100_00_en.pdf).

## Linux userspace witness (CI, no complete boot claim)

The pinned Linux 6.1 `tinyconfig` previously had `CONFIG_BINFMT_ELF` **disabled**. That would prevent a normal static ELF `/init` from executing even if the kernel completed early initialization. The dedicated CI workflow now explicitly enables `CONFIG_BINFMT_ELF=y` and builds `examples/guest-linux-init.S`, a tiny freestanding AArch64 Linux-syscall program with no libc. It packages this ELF as executable `/init` and a real `/dev/console` character device (major 5, minor 1) in a newc initramfs passed to the *existing* XI/Ether-backed Linux Image loader. If the machine reaches actual Linux user mode, `/init` writes `REVERSE_LINUX_INIT_OK` and then sleeps; absence of this banner is not a successful boot.

The workflow tests real upstream Linux at 500,000 transitions, resumes that exact RVG checkpoint for another 500,000, and compares the final binary checkpoint and concatenated console stream with a one-shot 1,000,000-transition reference. It additionally saves the 5,000,000-transition RVG checkpoint and a continuation up to 6,000,000 transitions (or earlier architectural stop), plus JSON diagnostics. Exact checkpoint equivalence validates continued *state*, not speed, full Linux boot, Android, or game compatibility. The ELF/CPIO builder is a test fixture, not a new guest runtime or a replacement for XI/Ether.

## PL011 IFLS compatibility gate

A Linux 6.1 guest from the verified XI/Ether 22M checkpoint reached 29,002,275 transitions, where the interpreter correctly stopped on an unimplemented PL011 IFLS write. The existing serial port now accepts and reads back legal 32-bit IFLS TX/RX trigger encodings (0–4), rejecting reserved encodings, and retains this configuration in a versioned machine checkpoint. Prior machine checkpoints with no IFLS setting keep their exact v2 bytes and restoration behavior. This does **not** claim cycle-accurate 16-byte FIFO occupancy/interrupt latency; the present serial transport still drains TX immediately and reports host-provided receive bytes promptly. Linux resumption uses the original XI/Ether state and must demonstrate further progress; no `/init` execution is presumed.

## Diagnosed halfword IFLS transaction at 29,002,275 guest transitions

The previous phase-3 replay stopped with `Invalid PL011 FIFO interrupt thresholds`, even though `x8=0x12` is a valid FIFO setting. Decoding the actual pinned kernel bytes at the guest PC revealed opcode `0x79000128` (`STRH W8,[X9]`): Linux used a **16-bit MMIO store**, but the earlier emulator patch required a 32-bit store. The IFLS register now accepts byte, halfword and word access, still rejects invalid trigger fields, and preserves the versioned checkpoint representation. Native regression tests exercise both the Linux 16-bit instruction and the prior 32-bit sequence, readback, rejection rollback, and checkpoint restoration.

The Linux probe now adds a read-only `next_instruction` JSON object with virtual/physical PC, opcode, all 31 registers and UART IFLS, sourced from the existing XI/Ether memory rather than copied RAM. A replay from the pinned failing checkpoint verifies a **single** next instruction and IFLS=18 before the longer boot test. This makes future ISA/device failures easier to identify without guessing from an instruction count or re-running Linux from zero. Successful Linux userspace still requires the actual `REVERSE_LINUX_INIT_OK` console marker; no Android boot is claimed.

## Memory-density audit at the next real Linux stopper

After the IFLS fix, [phase-3 CI 40773912490](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40773912490) executed the former rejected 16-bit store at transition 29,002,276 and then reached **29,059,889** before the configured 1,048,576 classical byte-override ceiling. The kernel did not enter `/init`. This is a **real guest state resource limit**, not a guest instruction decoder failure or a reason to report a completed Linux boot.

`scripts/profile-guest-memory.mjs` audits a saved RVG checkpoint **without executing or changing XI/Ether**. It checks the snapshot FNV-1a digest and section boundaries, then reports override counts, run lengths, touched 4 KiB pages, dense/full pages, physical address distributions, and a clearly labeled *hypothetical* dense-page payload estimate. A [dedicated CI workflow](../.github/workflows/linux-memory-profile.yml) profiles the exact stopped phase-3 artifact and preserves the JSON. This is a measurement tool only: no second memory authority, automatic capacity escalation, binary replacement, or compression algorithm is implied.

The engineering direction is to measure sparse-versus-dense page crossover and then implement a transactional, snapshot-compatible adaptive representation **within the same authoritative XI/Ether memory**, preserving quantum bindings, deterministic reads, rollback and previous checkpoint byte identity. Raising the override budget without solving density is a temporary diagnostic option, not a memory breakthrough or mobile performance claim.

## Adaptive Ether Memory without a fixed override-count quota (experimental)

The pinned Mundo X checkout is unchanged. REVERSE's reviewed compatibility overlay replaces only the *private classical override index within the existing InverseMemory* with copy-on-write 4-KiB pages. Pages begin as small sorted sparse vectors; at 128 stored bytes they promote to bitmap-backed dense pages, and return to sparse below 49. Canonical decimal addresses exceeding uint64 stay in the original arbitrary-precision sharded map. XI and the ARM64 interpreter retain the **one original QuantumRuntime authority**: quantum bindings, deterministic reads, revisions and atomic rollback.

The explicit **max_overrides=0** sentinel removes the fixed *count* quota, not real physical resource limits. The Linux probe opts in via MinimalGuest::enable_adaptive_memory() immediately after restoring its verified RVG checkpoint, without overwriting the original checkpoint. Ordinary REVERSE/legacy XI sessions keep their bounded policies.

For opted-in memory, compact **MXIM3** records store a 512-byte page-presence bitmap plus the actual present values, together with arbitrary-precision O records and existing Q quantum binding records, in the same versioned MXIM2 revision envelope. Old MXIM1 and RVG snapshots restore; bounded legacy profiles still generate byte-identical historic MXIM1 data. An opted-in *host development probe* may carry checkpoints up to 256 MiB instead of the default 48 MiB. Android app transport safeguards and physical phone memory budgets remain unchanged. Real constraints continue to include RAM, storage, CPU, logical-address validation and quantum resource budgets.

Verification: the new adaptive-memory regression stores **1,080,001** classical overrides beyond the former 1,048,576 ceiling, checks exact byte readback at page boundaries and addresses above 64 bits, preserves quantum bindings and old snapshots, checks state revisions and rejects failed transactions or malformed persistence. The dedicated adaptive Linux CI replays the SHA256-pinned 29,059,889-transition checkpoint and requires one successful transition across the former resource ceiling; it then records the next real boot blocker. The success marker REVERSE_LINUX_INIT_OK remains the only evidence of actual Linux userspace execution. Unlimited logical override count does not imply infinite physical resources, Android boot or game performance.

## Linux kernel STTR/LDTR: EL0 permissions from EL1 (2026-10-28)

The verified adaptive Linux run stopped at **29,415,473** instructions with opcode `0xf80008c3` at PC `0xffffffc00818607c`. An independent Clang/LLVM AArch64 assembler identifies the opcode as **STTR X3, [X6]** (store register, unprivileged). In Linux this can implement writes to userspace, and ARM requires the data access to use EL0 memory permissions when executed at EL1 with PSTATE.UAO=0; it does **not** lower the actual exception level. The original decoder rejected addressing mode 2. The fix adds the STTR/LDTR family for its existing integer sizes and uses EL0 permission checks for those accesses while retaining EL1 exception syndromes. No unimplemented UAO mode is claimed.

The regression uses the actual XI/Ether-backed AArch64 MMU: privileged-only mappings reject STTR/LDTR, ordinary privileged STR still works, a cross-page store with a protected second page commits no earlier bytes, and an EL0-writable mapping permits STTR followed by LDTR. Checkpoints and the quantum authority must remain unchanged on failed stores. The specific pinned Linux checkpoint is replayed separately in a targeted CI workflow; Linux userspace is only achieved when the real `REVERSE_LINUX_INIT_OK` marker appears. The kernel still lacks broad devices, platform services and graphics needed for Android or Wild Rift.

## SSBS-marked ERET on the first actual Linux EL0 return

After STTR and the native permission tests passed, the exact Linux checkpoint advanced from **29,415,473** to **29,455,637** guest transitions. It then reached `ERET` at `0xffffffc008011eb8` with `SPSR_EL1=0x1000`, `ELR_EL1=0x210148`. Bit 12 of SPSR_EL1 is **SSBS**. The previous decoder treated it as an invalid reserved bit and rejected a valid AArch64 EL0 return. The CPU is a sequential interpreter and does not advertise speculative execution or the SSBS mitigation; the bit is accepted as inert metadata during the exception return while other unmodeled SPSR bits are still rejected. EL0 access control remains unchanged and is **not** bypassed.

A native AArch64 fixture independently assembled with Clang sets SPSR_EL1, returns with ERET to an EL0 NOP, verifies the stack and privilege transition, verifies checkpoint restoration and rejects a truly reserved SPSR bit with exact transaction rollback. A dedicated CI resumes the pinned 29,455,637-step Linux state and verifies the actual ERET instruction, before continuing toward the real `REVERSE_LINUX_INIT_OK` userspace marker.

## CSDB speculative data barrier after Linux EL0 entry

The pinned ERET replay passed (95 native checks), returned to actual EL0 PC **0x210148** and subsequently advanced the real Linux kernel to **29,474,606 instructions**. That means the kernel reached the real /init ELF entry. It stopped at opcode `0xd503229f`, independently assembled and verified as **CSDB** (Consume Speculative Data Barrier), at `0xffffffc00801a168`. This instruction prevents unsafe speculation-dependent use of data on speculative CPU hardware. The REVERSE interpreter executes instructions sequentially with no speculative execution, so CSDB is represented as an architectural ordering hint without modifying XI/Ether bytes. Dedicated native tests verify PC progression, register preservation, exact checkpoint restoration, unchanged quantum/memory state and continuation. Do not infer that /init successfully printed or that Android booted: the UART marker is still missing.


## GKI auténtico: presupuesto separado de Linux (experimental, PR #42)

El antiguo `MinimalGuest::max_image=16 MiB` se creó para ELF pequeños y se
aplicaba por accidente también al Linux Image, al contenedor RVI1 y a la
validación de RVG restaurados. Ahora **ELF conserva su límite de 16 MiB**,
pero Linux admite un **kernel hasta 64 MiB**, un initrd hasta **64 MiB**,
y el contenedor inmutable Image+DTB+initrd tiene un máximo separado de
**128 MiB**. La geometría de RAM/arranque, los segmentos y el límite del
transporte RVG siguen siendo obligatorios; el perfil adaptable soporta
checkpoints hasta 256 MiB, no RAM física ilimitada. En el CLI de prueba,
Linux usa `enable_adaptive_memory()` en el único QuantumRuntime/InverseMemory.

El Image real de Android Common build 16707926 fue medido en **35,777,024
bytes**, con SHA256
`8d8cf5254f0823655e06ffee100d9c19abbf4ce33adb107b66da3fdb3742e0be`.
Una CI separada intentará cargarlo y ejecutarlo en el **mismo** intérprete;
solo aceptará progreso observado y un checkpoint RVG restaurable; cualquier
fallo ISA/dispositivo se documentará como el siguiente obstáculo. La app de
Android todavía tiene límites diferentes de transporte y **no** recibe por
este cambio la posibilidad de arrancar AOSP ni Wild Rift. El GKI sin ramdisk
genérico, vendor_boot, init_boot, particiones ni servicios no puede ejecutar
el Android `/init` real.
