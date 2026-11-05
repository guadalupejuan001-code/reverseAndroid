# C49: use the existing million-instruction transaction bound

The original MinimalGuest run API already permits one million instructions per
transaction and checks the timer/IRQ on every architectural step. C39's host
profile controller independently capped its opt-in chunk at 200,000. Increase
only that opt-in ceiling to the existing one-million API bound; the default
50,000 and the current workflow setting remain unchanged.

Larger transactions reduce repeated COW index copying. They preserve the same
XI/Ether authority, RVG/MXIM encoding, instruction budget, original decoder,
counter seed, 64-instruction cadence, disk and guest RAM. An unknown instruction
still rejects its transaction and the existing clean-prefix localization runs.
No instruction, interrupt check or guest time is skipped.

On authentic C47 stage 1, 20 million instructions took 11.553 s with C48's
200,000 chunk and 6.522 s with the existing core's one-million chunk. Both
produced the same complete RVG SHA-256
`0bee633570e51e7b3fa85205c79daa6f12937fe64627270d374437f77f76bff0`.
The earlier original guard/IRQ paths took 17.167 s on the same instruction
budget. These are execution timings, excluding attach/restore/save.

The diagnostic benchmark now accepts an explicit optional host chunk, preserving
its existing default and maximum instruction budget. It remains diagnostic and
never produces a resumption contract. Actual continuation additionally uses the
unchanged source admission and clean-budget contract checks.

The compiled profile controller also completed the same 20-million-instruction
budget with `REVERSE_PC_C39_CHUNK=1000000`, exit code zero, accepted clean
boundary and the identical RVG hash above.
