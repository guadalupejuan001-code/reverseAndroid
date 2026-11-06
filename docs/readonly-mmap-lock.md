# Read-only mmap semaphore context

The first independently inspected C89stage6 contains55 SystemServer7354 threads. Four `package-parsing` workers, TIDs8085–8088, are waiting in `rwsem_down_write_slowpath` / `down_write_killable` during `mmap`. The main thread waits on a new private futex. This establishes a concrete memory-mapping wait, without identifying its lock holder or proving a permanent deadlock.

The supplemental probe reads `mm_struct.mmap_lock` using the exact pinned kernel's BTF member offsets. It records raw count and owner words, a validated task pointer if available, and at most2048 waiters with full forward/reverse backlinks. Invalid pointers or unavailable private waiter types remain explicitly unverified. The observer checks the complete original snapshot digest after this read; no guest instruction or UART input is executed.

A reader-flagged owner is a debugging hint and can refer to a previous reader. It must never be used alone to identify a current or sole holder, or to justify a scheduler change. Raw words and the BTF-derived offsets are retained alongside any decoded task. Reference for these generic owner semantics: [Linux rwsem implementation](https://github.com/torvalds/linux/blob/master/kernel/locking/rwsem.c). This reference does not replace the guest's pinned BTF or actual observations.

Tests cover an actual bounded writer/waiter fixture, stale-reader semantics, an empty semaphore, corrupt backlinks and unchanged input memory. All870 imported original files remain unchanged.
