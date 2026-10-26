NVNET MAC-address width regression
=================================

Run on a little-endian Linux host with GCC and Python 3::

    python3 tests/xbox/nvnet_mac/test-nvnet.py . /tmp/nvnet-mac-test

This extracts the actual MMIO handlers, register helpers and TX dispatch path.
DMA, PCI IRQ and MDIO execution endpoints are modeled. Tests cover aligned
1/2/4-byte accesses to both MAC-address storage words, all neighboring register
bytes, absence of IRQ/MDIO/DMA side effects, low-lane W1C, IRQ mask updates,
32-bit TX kick/reset and MDIO dispatch, plus retained rejection of unaligned
halfwords and upper-lane accesses to side-effect registers. ASan/UBSan enabled.

Current upstream fails the MAC byte sequence. The historical global assertion
relaxation also admits upper-byte IRQ_STATUS writes that bypass its W1C handler
and become plain writes. This candidate instead handles only the MAC storage
bank through the existing size-aware helper; it leaves the general assertion
and all other register handlers unchanged. It does not establish hardware
semantics for arbitrary partial accesses to command/status registers.

Related issue: https://github.com/xemu-project/xemu/issues/2305
PR #3026 was closed unmerged; a maintainer indicated an intended proper fix.
Coordinate with that work before any competing submission. No message or PR
has been sent as part of this investigation.

Smallest hardware parity probe: a source-available XBE saves the MAC registers,
writes known byte and aligned halfword patterns, reads each width and neighbors,
and restores the saved MAC. Run only under an authorized hardware test plan.
Fresh Cromwell startup and ordinary networking on current upstream also remain
unverified. This test is not a full MemoryRegion/device dispatch test.

Agent assistance: Codex / GPT-6; disclose this in a future PR and independently
review and defend the implementation.
