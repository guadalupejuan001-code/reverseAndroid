# C33 · corrección ARM64 USRA/SSRA y reanudación segura

## Fallo real observado

La [primera ejecución C33](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41340518244) restauró correctamente el checkpoint C32 y ejecutó **51,092,697 instrucciones nuevas**. Se detuvo por una instrucción no soportada, **no** por un reboot del invitado:

- PC: `0x7065ccad60`, excepción de usuario EL0 en el proceso `android.hardwar`.
- Opcode: `0x6f7f1440`; desensamblado con LLVM: `usra v0.2d, v2.2d, #1`.
- Contador final: **13,918,491,536** instrucciones.
- `stop_reason: architectural_stop`, `error: Unsupported AArch64 instruction 0x6f7f1440 at 0x7065ccad60`, salida nativa 2.
- Checkpoint de fallo (evidencia **NO reanudable como fuente limpia**):
  `da9cc4ba52e8e83ff7e920b595005c7d6803ccf2dbf88ba902bbceff4232cb63`.
- SHA-256 del reporte de fallo: `78a586876e49ac6c870810df5fbb3f24ae39653683c0583bdb053edb2c0d477b`.
- [Evidencia preservada](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41340518244/artifacts/13096974617). No existe contrato de salida limpia y la política de continuidad debe rechazar esta imagen.

## Corrección dirigida a la ISA original XI/Ether

`src/minimal_guest_simd.inc` reconoce SSRA/USRA cuando la máscara del opcode
es `(instruction & 0x9f80fc00) == 0x0f001400`, con `immh != 0`.
Decodifica la anchura de cada elemento (8, 16, 32 o 64 bits) desde `immh:immb`,
la cantidad de bits de desplazamiento `2*element_width - immh:immb`,
el signo con el bit U, y el ancho del vector con Q.

Operación por elemento, módulo la anchura:

```text
SSRA: Vd[lane] := Vd[lane] + ASR(Vn[lane], shift)
USRA: Vd[lane] := Vd[lane] + LSR(Vn[lane], shift)
```

Para desplazamientos iguales al tamaño de elemento, la rama evita los
desplazamientos C++ indefinidos (64) y entrega el resultado correcto de extensión
de signo o cero. Se rechaza la disposición 1D no asignada. El código copia
la fuente y acumulador antes de escribir para soportar Rd==Rn, y preserva
registros ajenos, NZCV, FPCR, FPSR y el estado XI/Ether.

La suite nativa `tests/minimal_guest_shift_accumulate_test.cpp` cubre el
opcode exacto, aritmética por bit independiente, todos los tamaños y signos,
desbordamientos, extremos, aliasing de registros, serialización/restauración,
forma reservada, y trap FP/SIMD privilegiado. Prueba del intérprete real,
no una simulación de Android.

## Política para reintentar

Repetir desde **el último checkpoint limpio C32** de
13,867,398,839 instrucciones, SHA
`721a228423e55810f11dc4295c64e41feca1fb2121d6db8a0666a6e219ad1a53`,
después de autenticar su contrato de cuatro archivos y la imagen C27.
La CPU reconstruida usa la misma imagen de kernel, disco, counter seed y
configuración; solo cambia el decodificador SIMD para ejecutar una instrucción
antes rechazada.

No reutilizar el checkpoint terminal como si fuera sano. No cambiar AVB,
SELinux, los medios originales ni atribuir pantalla o boot. Una nueva
continuación será válida solo si termina limpiamente y conserva una
nueva procedencia reproducible de resultados.

## Segundo stop ISA de C33 con USRA ya resuelto

La siguiente ejecución desde C32 [CI 41341874317](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41341874317) superó el bloqueo original de USRA: llegó a **13,919,192,922** instrucciones (+51,794,083 desde C32), **701,386 instrucciones más allá** del primer stop.

El nuevo stop independiente es `0x6ee0b821` en `0x7065cd4940`,
que LLVM desensambla como **`neg v1.2d, v1.2d`**. La CPU fue detenida por el intérprete con `architectural_stop` y no por un reinicio fatal del Android invitado. Su checkpoint `ba84eedeec3cb47a53db9028f7e9b632d4e3aff508cada23a568b0c1bdbcabe1` **no** se acepta como frontera sana.

La misma rama incorpora ahora **NEG vectorial** en la CPU XI/Ether,
para B/H/S/D (según disposición Q), por elemento aritmético módulo 2^n.
La suite nativa verifica negativos extremos, `Rd==Rn`, preservación
NZCV/FPCR/FPSR, memoria XI/Ether, traps y rechazo de la disposición 1D no asignada.
La nueva prueba CI debe validar **ambas** correcciones (USRA/SSRA + NEG)
antes de reanudar el **checkpoint limpio de C32**, sin utilizar ninguno de
los dos checkpoints terminales como fuente.
