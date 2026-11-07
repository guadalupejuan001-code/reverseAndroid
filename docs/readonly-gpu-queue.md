# Read-only GPU split-ring observation

`tools/reverseandroid-gpu-queue.mjs` reads at most32 recent distinct descriptor heads from the original GPU control queue and at most1MiB of original guest RAM. It records the current resource table, scanout, retained request/response headers, resource identifiers, queue indices and bounded descriptor chains. The observer rejects indirect descriptors, cycles, oversized buffers and invalid physical addresses. It never executes a guest instruction or writes guest/device state.

Completed split-ring entries can point to descriptor buffers that the guest has subsequently freed or reused. The result explicitly marks retained buffers as **not proof of historical execution**. Matching nonzero fence flags and the same fence identifier are reported as matching retained headers only. They do not establish a previous allocation failure or justify a resource-capacity, format, codec or native change by themselves.

The supplemental thread observer uses the unchanged, authenticated original kernel observer and verifies the entire original snapshot digest after observation. Its optional `--gpu-queues` report is read alongside full original epoch preflight, authentic source metadata and unchanged source-file validation. No supplemental result replaces original admission, original boot completion or actual UI review.

Independent synthetic tests cover a missing resource with a matching fenced response across queue-index wrap, a stale response with a different fence, descriptor cycles, indirect descriptors and invalid physical addresses. Fixture RAM must remain byte-identical after the read.
