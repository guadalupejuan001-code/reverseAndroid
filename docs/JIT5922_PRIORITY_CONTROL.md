# Isolated guarded priority control for the recovered Zygote

Select the latest fully admitted C87 stage 5 at the time of this selection:
114,147,727,998 instructions, checkpoint SHA-256
`5e8ddb757fee6cbc030230f2dfc532dfb2e08648c9e00f4020335f7b28ff109c`.
The source was restored from all four independently hashed CI transport ZIPs,
their inner pieces, and the whole original RVG. Original epoch preflight
passed. C87 continues as the unchanged-input reference after this explicit
selection; later C87 states do not belong to this new input schedule.

The supplemental original-kernel probe finds the original Zygote PID 5629,
its verified `Jit thread pool` TID 5922, UID 0, runnable, with normal/static
priority 139 (nice +19). Between the earlier 110-billion boundary and the
113-billion boundary, the main accumulated 5,424,691,000 ns of user time;
the JIT's user/system counters were unchanged and its scheduler runtime rose
by only 1,166,000 ns. This motivates a controlled scheduling comparison; it
does not establish that the current main is deadlocked or that this is the
only limit on completed boot.

The single modeled PL011 input reuses the previously measured C81 Toybox
command, substituting the actual newly recovered PID/TID and distinct reply
markers. The guest itself rechecks the thread name, TGID, UID and nice +19
before asking Toybox for a relative renice increment of -19. It then reads
nice back and emits the result. A controller never treats the echoed command
as a reply. Source guard or actual reply failure stops this control, while
the original admitted state and every new valid checkpoint are retained.

`scripts/run-jit5922-control.py` verifies the exact source, native executable,
complete inherited console and seven inherited peripheral events. It sends
one new authenticated priority event on the first stage; later stages send
zero new bytes. The first native budget is 500 million instructions and
subsequent budgets are at most one billion. The existing native engine,
original restore/roundtrip, kernel, live properties, peripheral checks, RAM,
seed, 64-instruction cadence, IRQ checks and compact codecs remain in use.
There is no new power cycle.

Each valid output contains its original admission and a separate
`jit5922-control-proof.json`, the inherited seven events, the new event and
the authenticated complete console. Guarded priority completion, actual
SystemServer, original completed boot and a usable UI response are reported
separately. The control does not claim completed Android merely because an
instruction budget or the priority command succeeded.

The workflow is isolated to its experiment branch, preserves a full
checkpoint before advancing, and stops when the original boot plus current
peripheral checks pass. Authentic display inspection and a separately bound
UI response test remain subsequent requirements.

## Measured result through stage 2

The local original-engine replay and the independent CI run
[42259037815](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42259037815)
produced identical complete snapshots at both the first 500-million boundary
and the next one-billion boundary. Console bytes, framebuffer bytes and the
original kernel observation also match. Stage 2 is at 115,647,727,998
instructions with SHA-256
`7b358773528ecf8fb89c5236863a9f5f83d95e2059d629f7dfbc0b430cce65c6`.

Android emitted `REVERSE_JIT5922_CONTROL_DONE NICE_0`. A fresh, read-only
original-kernel probe independently finds the same kernel task pointer,
TGID 5629, TID 5922, name and UID 0, with normal/static priority 120
(nice 0). Its user/system CPU counters increased by 15,372,000 ns from
the selected source, and its saved user PC advanced into `libart.so`.
The scheduler runtime increased by 12,953,000 ns. Both counter types are
reported separately because Linux accounting is sampled at this boundary.

The original C46 gate and the producer's live peripheral checks pass.
The outer shell's final RC marker is still pending at this stage.
SystemServer, live completed boot and usable UI response remain unverified.
This measurement establishes that the guarded scheduling change executed
and the JIT advanced; completed-boot acceleration has not been measured.

See [the complete measurement and producer bindings](evidence/jit5922/stage2-independent-results.json).
The full fresh thread observation is preserved as authenticated gzip/base64
transport in `evidence/jit5922/stage2-threads-readonly-transport.json`.
This transport encodes a read-only report and does not change any guest
memory, executable, snapshot format or continuation pointer.
