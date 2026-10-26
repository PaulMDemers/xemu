CPU-write surface eviction policy investigation
===============================================

This branch preserves the guarded experimental build003 policy against current
upstream. It is NOT recommended for upstream submission as a demonstrated stock
emulator fix. Historical build002 introduced lost draws between BEGIN and END;
build003 corrected that regression. Stock controls also passed the retained
pixel tests. No measured performance improvement or stock failure requiring
eviction has been established.

Run an isolated policy test on Linux with GCC and Python 3::

    python3 tests/xbox/surface_eviction/test-surface.py . /tmp/surface-policy

Actual callback, overlap predicate and pending-download worker are executed.
Locks, scheduling, lists, GL downloads/deletion and memory ownership boundaries
are modeled. Nine checks cover preserve-before-store, active primitive binding,
clean active primitive, BEGIN injected between request and worker, non-vCPU
caller, reads, clean surfaces, overlapping modeled bindings and range endpoints.
Deleting either primitive guard must fail its corresponding mutation test::

    python3 tests/xbox/surface_eviction/test-surface.py . /tmp/surface-request-broken drop-request-guard
    python3 tests/xbox/surface_eviction/test-surface.py . /tmp/surface-worker-broken drop-worker-guard

Expected failures above are test sensitivity checks. The injected schedule is
not a naturally observed race. The model does not prove simultaneous watches
can occur in the real renderer, and it does not implement Z16, swizzle, real GL
formats, TLB invalidation, reset or concurrent CPU/renderer execution.

Current upstream already includes overlapped-blit handling (578094b834) and
DIRTY_MEMORY_NV2A download tracking (25e1ee2fd5). Related open work includes
#2965, #2984, #3096 and #2514; none makes this eviction policy an established fix.

Next action: identify a current-stock failure or a measured optimization benefit
before expanding this change. If justified, use fresh source-available XBE
fixtures for overlapping pitches, Z16, swizzled surfaces and explicit emulator
reset; retain exact pixel oracles and the failed build002 evidence. No closed
historical guest may be relaunched or used as a new parent.

Agent assistance: Codex / GPT-6. This is an investigation branch, not a claim
of general game compatibility, safety, performance or physical qualification.
