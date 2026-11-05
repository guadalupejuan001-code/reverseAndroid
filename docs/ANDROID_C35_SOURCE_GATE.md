# Android C35 — control de admisión del checkpoint C34

## Estado real
C33 aprobó CI con checkpoint SHA-256 `740a564577c991b24d131f4335806ad491413bfa00d8bdeb6942973d46fa348c` y **14,220,348,839** instrucciones; `odsign` seguía activo, sin evidencia de Zygote/SurfaceFlinger.

C34 está ejecutando el ciclo de continuación desde C33 en GitHub Actions:
https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41345457550

**No se puede seleccionar un checkpoint C35 sin que C34 termine exitosamente y produzca su artefacto autenticado.** Este PR prepara únicamente la puerta de entrada de C35. No afirma que C35 ya esté ejecutando CPU ni inventa estado visual Android.

## Reglas del gate

- Exigir `preflight` de los cuatro archivos reales `current.rvg`, `current.json`, `result.json`, `process-outcome.json`, con el contrato resultante de C34.
- Exigir commit productor exacto `75f92559d488ca13f81e51acc8157aa650ff86e0` de la ejecución C34 ID `41345457550`.
- Exigir auditoría de etapa C34 `actual-stage.json` del mismo run, atada al SHA y al contador del checkpoint recibido.
- Exigir frontera limpia: `boundary-summary.json.clean_completed === true`, progreso de instrucciones > C33, sin error o parada arquitectónica.
- Rechazar mezclas de checkpoints, firmas cambiadas, cuenta falsa de instrucciones, reinicios, falsos boot-completed y declaraciones visuales que aún no se demostraron.
- No confundir la prueba del host con ejecución Android. El gate únicamente autoriza el siguiente ciclo cuando llegan los artefactos.

## Próximo trabajo real

Cuando C34 entregue una salida válida, fijar su SHA-256, inspeccionar `odsign`, Zygote, `system_server`, `surfaceflinger`, propiedad `sys.boot_completed` y framebuffer, y crear un workflow de continuación desde **esa** frontera exacta. Si falla C34, diagnosticar el fallo y no continuar desde un estado terminal.

Sin aprobación CI del gate y sin una salida C34 limpia, no se admite C35.
