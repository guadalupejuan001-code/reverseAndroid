# Explicit input for the existing boot epoch

The epoch runner can accept one explicit, byte-pinned console input plan. It never reads ambient input environment variables. Existing no-input runs still deliver no new bytes. The original PL011 receive queue, original `guest_console_input_event`, original CPU, original memory and codecs remain the authorities.

The native runner validates the complete source/report, original restore roundtrip, RAM, original disk and clock before reading and authenticating optional input. The supplied file must be nonempty, at most 4096 bytes, and match its exact SHA-256. A second event is refused while previous receive bytes remain pending. The input is delivered through the original `console_input` API and recorded with its actual source instruction count and counter phase. No guest instruction is counted merely for queuing bytes; no Android property or RAM location is written by the input helper.

The host controller additionally binds a plan to one exact source snapshot and instruction boundary. For a `ui_response_test`, it requires the unchanged original completed-boot gate to have passed, an authenticated exported before frame, and that frame's exact PPM hash. A UI command cannot substitute for boot evidence. An explicitly selected `diagnostic_console` plan has the same source/byte authentication and does not claim UI verification.

Example plan, to be populated only after observing the actual source and UI:

```json
{
  "schema": "reverseandroid-authenticated-console-input-plan-v1",
  "purpose": "ui_response_test",
  "source_checkpoint_sha256": "ACTUAL_SOURCE_SHA256",
  "source_instructions": 0,
  "input_file": "actual-input.bin",
  "input_bytes": 0,
  "input_sha256": "ACTUAL_INPUT_SHA256",
  "before_frame_ppm_sha256": "ACTUAL_AUTHENTICATED_FRAME_SHA256"
}
```

The zeros and uppercase values above are placeholders and are rejected as a real input plan. No UI input has been sent by adding this support. Actual Android boot, actual UI content and its response must still be separately verified.

The `run` command accepts `instruction-budget [input-plan.json|-] [host-wall-ms]` after its existing source/output/native/disk/frame/commit arguments. The wall budget bounds host execution only; it does not alter guest RAM, clock frequency, seed, cadence, IRQ checks or serialized model state. A clean host wall boundary remains subject to all original roundtrip, process/report, kernel, property and frame checks. Local execution should run one bounded stage per host command so the host command's lifetime cannot interrupt a multi-stage chain.

For interaction, use a native binary built from the updated epoch runner. Older preserved binaries remain useful for no-input execution and reject the extra input arguments. `host-policy.json` records the actual binary SHA-256 and selected host budget on every new stage. The original reference binary remains preserved.

Validation includes source/frame/byte mismatch rejection, rejection of UI tests before completed boot, rejection of unrequested native events, and a native fixture showing that an explicit event changes only the modeled receive queue. The fixture verifies unchanged CPU registers, unchanged original RAM/quantum snapshots, byte-identical absent-input behavior, and the complete original snapshot roundtrip. These tests are not an actual Android UI-response claim.
