PRAMDAC timing storage investigation
====================================

Run the actual read/write, reset and preload/postload functions with modeled
containing-device infrastructure on Linux with GCC and Python 3::

    python3 tests/xbox/pramdac/test-timing.py . /tmp/pramdac-host

The aligned 32-bit readback/isolation test performs 409601 checks; reset adds
20 and preload/postload adds 30. Current upstream fails readback; removing the
new reset or preload assignments fails the corresponding test. These tests do
not qualify subword accesses, reset synchronization or raster/field behavior.

The former private patch used NV2A migration version 4. Upstream now uses version
4 for PTIMER and version 5 for shader carry state. This port appends latches at
version 6 and leaves all existing schema entries in their original order.
Historical private version-4 snapshots are incompatible with upstream version-4
layout and are NOT accepted as validated input to this port.

Generate a real QEMU VMState serializer test in a configured current-source build::

    python3 tests/xbox/pramdac/test-pramdac-vmstate.py . BUILD /tmp/pramdac-host /tmp/pramdac-vmstate

Run the produced test-vmstate-pramdac binary on its target OS with --tap. This
adds a projection of the actual seven PRAMDAC migration fields to the existing
QEMU VMState unit suite. Versions 1 through 5 clear new latches and version 6
round-trips their exact values. This projection omits the rest of NV2A and is
NOT qualification of an entire emulator snapshot or live migration. The first
local projection fixture omitted the QEMU EOF byte and failed; the corrected
fixture passes all 24 VMState tests. Reset/preload calls in host tests mock FIFO
locks and renderer completion, so real reset/snapshot lifecycle needs testing.

Register offsets agree with nouveau's nvreg.h definitions for the NV family:
https://kernel.googlesource.com/pub/scm/linux/kernel/git/will/linux/+/fdb9c3cd5124c9a6e4c824ed2bca5b4602e84a1a/drivers/gpu/drm/nouveau/nvreg.h
This corroborates addresses, not Xbox-specific masks, reset values or timing.
Neighboring PRAMDAC timing registers are already stored but many are omitted
from the existing migration schema; this patch does not silently fix those.

Verdict: plausible register-storage fix with historical guest evidence, still
missing direct hardware readback/mask/reset evidence and full-device snapshot
qualification. Next: a source-available hardware/emulator readback probe, then
fresh paused/running save-load and reset fixtures on this version-6 port.
No hardware execution is authorized by this note. No raster timing claim.

Agent assistance: Codex / GPT-6; human review and disclosure required before PR.
