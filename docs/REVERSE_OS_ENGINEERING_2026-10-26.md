# REVERSE OS — engineering acceptance contract

The machine exists as an XI runtime, not as a standalone CPU or hardware OS. The Android REVERSE OS workstation makes the real runtime usable without requiring a PC or terminal, while keeping the original computational architecture.

## Execution path

\`\`\`
Human → XI source (programs/*.xi) → native JNI → QuantumRuntime.execute()
                                            → InfiniteCore / Ether
                                            → Inverse Ether Memory
                                            → CompactCore / AdaptiveReduced / Reduced
                             success → RVPC2 AtomicFile → UI shows actual JSONL
                             failure → original RVPC2 remains unchanged
\`\`\`

There are no second state authorities, hidden machine-learning models, or invented performance numbers. Native \`session.hpp\` encodes the exact RVPC2 envelope from \`src/reverse_pc.cpp\`, with size checks, FNV-1a integrity and restore round-trip. State edits are transactional within QuantumRuntime; Android only publishes the new state after it is successfully serialized and checked.

## What is a workstation, what is an OS?

- **Verified machine feature:** bounded classical XI execution, 16 signed 64-bit registers, loop/branch and arithmetic, quantum gates T/T† and Ether on one owner, linked memory, durable snapshots.
- **Delivered host feature:** Android GUI, program editor, local console and private file browser, import/export, telemetry. Android itself remains the host OS.
- **Not yet delivered guest OS feature:** binary executable format, general filesystem, process scheduler, virtual I/O devices and process isolation. Do not call the emulator a standalone operating system.

## Next milestones

1. Compile text XI to a typed, versioned compact binary format, verify its hash and target semantics before any state mutation; prove textual vs binary equivalence.
2. Add guest file operations through capability-checked virtual filesystem and fully atomic process/IO commits.
3. Add multiple isolated XI processes with runnable/blocked states, bounded step quanta, deterministic scheduler, independent register snapshots, heap and device budgets.
4. Create a guest system-call ABI and expose device ports through strictly mediated adapters. Keep Android UI as a host view, not a second machine.
5. Evaluate Ether classical state transformations as direct computational operations, with measured performance against classical references; only investigate FPGA when an actual advantage supports it.

## Tests
- Linux host runs the same C++ quantum owner, tests GCD=21, RVPC2 round-trip, inverse-memory byte persistence, corruption detection and a 128-qubit entangled gate chain.
- Android CI builds x86_64+ARM64, checks signer/permissions, installs the APK and runs an XI program from the graphical editor in an actual emulator; its output must be a real native result \`r0=21\`.
- This foundation is a practical UI and runtime environment, not proof of a new physics model or faster graphics.


## v0.3: shell horizontal con GPU

La interfaz Android replica los patrones familiares de escritorio (inicio, ventanas, barra de tareas y accesos directos). El fondo orbital animado usa GLSL 300 ES sobre el pipeline GLES3 adaptativo de `space-glitch-android`, sin modificar Ether, InfiniteCore ni su autoridad. Es una prueba de rendimiento gráfico de la GPU anfitriona, no computación cuántica. CI mide FPS reales y verifica cambios entre capturas del fondo. La ejecución de XI permanece en QuantumRuntime y el estado RVPC2 es transaccional.
