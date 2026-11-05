# Prompt para ChatGPT Work — REVERSE-PC

Actúa como ingeniero principal y agente de investigación de rendimiento. Trabaja
sobre mi repositorio de GitHub `guadalupejuan001-code/REVERSE-PC`, con Mundo X
solo como dependencia técnica, para continuar construyendo una arquitectura
computacional compacta y programable, que estoy desarrollando como mi
supercomputadora de software.

**No reinventes mi proyecto ni cambies su dirección.** Primero lee en GitHub:

- `docs/WORK_HANDOFF_2026-10-25.md` (hallazgos con métricas y criterios de aceptación).
- `src/adaptive_reduced.hpp`, `src/factorization_benchmark.cpp`,
  `src/reverse_pc.cpp`, `docs/architecture.md`, `docs/factorization.md`.
- PR #6 (factorización fusionada), PR #7 (batería de arquitectura) y PR #5
  (auditoría anterior pendiente; conciliar sin sobrescribir mejoras).
- El backend canónico fijado de Mundo X, commit
  `e2778d962af22f6f1a0794058dd4685fc2573966`.

**Empieza a trabajar, no te limites a sugerencias.** Tu primera tarea es
reproducir los benchmarks de los cuatro problemas confirmados:
(1) bloques que no se vuelven a separar tras CX/CX;
(2) acumulación de `history_` incluso con puertas que se cancelan;
(3) reconstrucción/replay costoso al activar Reduced; y
(4) coste de copiar metadatos por instrucciones XI individuales. Comprueba
los datos de CI y sus limitaciones antes de cambiar código.

Después implementa mejoras pequeñas y medibles, con prioridades:

1. Recuperar la factorización cuando un bloque vuelve a ser separable,
   sin pérdida de fases, norma, entrelazamiento ni amplitudes relevantes.
2. Reducir el historial y el coste del cambio de backend sin perder
   reproducibilidad, rollback ni precisión; investigar conversión exacta
   de estado antes de sustituir el replay.
3. Optimizar las copias de metadatos y consultas de XI preservando
   atomicidad, revisión observable, RNG y compatibilidad de instantáneas.
4. Planear e integrar cuidadosamente el motor factorizado/Reduced en
   `QuantumRuntime`, `Inverse Ether Memory` y lenguaje XI, para
   habilitar una computadora programable, no solo una demo independiente.

En cada cambio: crea una rama y PR, ejecuta tests existentes y nuevos,
compara resultados con oráculo independiente, usa ASan/UBSan y registra
tiempo mediano/p95 y RAM residente real. Ensaya circuitos separables,
entrelazados, con puertas T, cancelaciones, cargas largas y estados
restaurados. Ejecuta los mismos programas con igual precisión y hardware.
Conserva el código original y evita fusionar PRs con CI fallida.

No presentes 1.000 qubits lógicos simulados como hardware cuántico real,
ni memoria lógica como RAM física, ni una mejora en un circuito como
aceleración universal. No agregues apps gráficas, APKs ni renderizadores
a menos que sean necesarios para una prueba computacional concreta.

**Entrega:** cambios verificables en GitHub, benchmarks antes/después,
hallazgos y límites reconocidos, pruebas reproducibles, y un README/plan
de la siguiente iteración. Realiza el mayor trabajo posible en la sesión
actual en vez de detenerte con propuestas vagas.
