# Actual UI response after original Android boot

The final test uses a genuine Android `input touchscreen swipe`, an authenticated
before frame and the original epoch runner. It is prepared only after the
original C46 gate verifies live PID1 BOOT1, a genuine live SystemServer and a
visually reviewed usable display. At the C88 stage7 boundary, these requirements remain
unverified; no UI input has been prepared or executed.

`scripts/prepare-ui-response-input.py` checks the original boot evidence, snapshot
and frame hashes, instruction boundary, coordinates and actual before-frame
review. Its input plan uses the existing original `ui_response_test` mechanism;
the epoch runner independently re-admits the same complete source before input.

`scripts/prepare-booted-ui-response-state.py` verifies that whole original source,
its authenticated campaign proof, all eight prior input events, entire original
console, the real priority reply, enforcing policy and fresh reciprocal peripheral
links. The old console is retained as observation history and is never sent back
into UART.

`scripts/run-booted-ui-response-stage.py` submits the new gesture exactly once.
Every later bounded stage supplies zero new UART. Each native stage is at most
250M instructions with the original900000ms host cap, original whole-snapshot
roundtrip, C46 admission, process/report binding, kernel and live-property gates,
unchanged957f22e2 native, disk,2GiB logical RAM, seed, clocks, cadence and codecs.
The complete snapshot must be preserved before another stage or older duplicate
release, using the same checkpoint preservation action as the boot campaign.

The actual `REVERSE_UI_RESPONSE RC_0` reply is parsed independently from PTS
evidence. Echoes, partial lines, repeated replies and nonzero errors cannot pass.
Original before and after PPM and RGB bytes are authenticated; the difference
calculation reports changed pixels without assigning them meaning. A missing
scanout remains an unresolved visual result and does not discard a valid native
checkpoint.

The stage's `ready` output means that the real command succeeded, original BOOT
and SystemServer remain verified, peripherals are fresh and an authentic frame
changed. It stops further native stages for actual image inspection. The stored
`ui_response_verified` remains false until that inspection confirms a meaningful
response, such as the notification shade opening. Both authentic images and their
source/input/checkpoint bindings must accompany the final evidence. A changed
frame and a successful shell command alone do not establish a usable response.
