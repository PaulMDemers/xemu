NVNET transmit batch regression
===============================

Run from the source root on a little-endian Linux host with GCC and Python 3::

    python3 tests/xbox/nvnet_tx/test-nvnet.py . /tmp/nvnet-tx-test

The runner extracts the actual TX function, register accessors, descriptor
load/store and ring-pointer helpers from this checkout. Guest RAM, packet output
and PCI IRQ delivery are modeled. GCC uses ASan/UBSan and warnings as errors.
This is an isolated source test, not a QEMU MemoryRegion dispatch, DMA address
translation, network backend, guest workload or physical hardware test.

The current-upstream control (25e1ee2fd59a451f2ab57e825b4f8a5414220c8b) fails
batch, wrap, full-ring and maximum non-overflowed ring draining. The candidate
passes these and split-kick fragments, invalid ownership termination, all three
transmission gates, flag completion, exact buffer capacity and out-of-ring next
pointer normalization. Oversized fragment accumulation must still assert; this
test does NOT claim robust malformed-descriptor handling.

Two existing limitations are observed, not approved: a programmed ring size of
65536 truncates to zero in get_tx_ring_size(), and length 0xffff truncates to a
zero-length transfer. The max-ring case additionally drains 65535 descriptors
without wrapping the work bound. Keep overflow/error handling as an independently
reviewed follow-up; do not remove assertions to make malformed inputs pass.

Historical matched Linux guest trials support the batching diagnosis, but no
guest trial on this new upstream base is claimed. Related issue:
https://github.com/xemu-project/xemu/issues/2910
Open RX recovery PR #3107 concerns a different direction of packet flow.

Next qualification: a source-available XBE queues multiple complete packets and
split/wrapped fragments before one kick, checks descriptor ownership and TX IRQ,
then repeats with a fresh current-upstream emulator control and candidate. A
normal networking workload and a QEMU-integrated version of this test remain
useful gates before submission. No physical execution is authorized by this note.

Agent assistance: Codex / GPT-6. Human review and ownership are required before
submission; include that declaration in any future PR.
