/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Deterministic memory/packet boundary for actual NVNET source functions. */
#include "hw/net/mii.h"
#include "hw/xbox/mcpx/nvnet/nvnet_regs.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MMIO_SIZE 0x400
#define GET_MASK(v, mask) (((v) & (mask)) >> __builtin_ctz(mask))
#define NVNET_DPRINTF(...)                \
    do {                                  \
        if (0)                            \
            fprintf(stderr, __VA_ARGS__); \
    } while (0)
#define trace_nvnet_update_irq(...) ((void)0)
#define trace_nvnet_desc_store(...) ((void)0)
#define trace_nvnet_cant_tx(...) ((void)0)
#define trace_nvnet_tx_dma(...) ((void)0)
#define trace_nvnet_reg_read(...) ((void)0)
#define trace_nvnet_reg_write(...) ((void)0)
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Fixture byte conversion is for little-endian test hosts only
#endif
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le16(x) (x)
typedef uint64_t hwaddr;
typedef uint64_t dma_addr_t;
struct RingDesc {
    uint32_t buffer_addr;
    uint16_t length, flags;
};
typedef struct NvNetState {
    _Alignas(4) uint8_t regs[MMIO_SIZE];
    uint32_t phy_regs[6];
    uint8_t tx_dma_buf[TX_ALLOC_BUFSIZE];
    uint32_t tx_dma_buf_offset;
    uint8_t memory[1024 * 1024];
    unsigned sent, loads, writes, irq_updates, mdio_reads, mdio_writes;
    size_t lengths[65536];
    uint8_t packets[4][TX_ALLOC_BUFSIZE];
    bool irq;
} NvNetState;
typedef NvNetState PCIDevice;
#define PCI_DEVICE(s) (s)
#define NVNET(s) ((NvNetState *)(s))
static void pci_irq_assert(PCIDevice *s)
{
    s->irq = true;
    ++s->irq_updates;
}
static void pci_irq_deassert(PCIDevice *s)
{
    s->irq = false;
    ++s->irq_updates;
}
static void pci_dma_read(PCIDevice *s, dma_addr_t a, void *p, size_t n)
{
    assert(a + n <= sizeof(s->memory));
    ++s->loads;
    memcpy(p, s->memory + a, n);
}
static void pci_dma_write(PCIDevice *s, dma_addr_t a, const void *p, size_t n)
{
    assert(a + n <= sizeof(s->memory));
    ++s->writes;
    memcpy(s->memory + a, p, n);
}
static void send_packet(NvNetState *s, const uint8_t *p, size_t n)
{
    assert(s->sent < 65536 && n <= TX_ALLOC_BUFSIZE);
    s->lengths[s->sent] = n;
    if (s->sent < 4)
        memcpy(s->packets[s->sent], p, n);
    ++s->sent;
}
static void mdio_read(NvNetState *s)
{
    ++s->mdio_reads;
}
static void mdio_write(NvNetState *s)
{
    ++s->mdio_writes;
}
static void dump_ring_descriptors(NvNetState *s)
{
    (void)s;
}

/* ACTUAL_FUNCTIONS */

static NvNetState s;
static void fresh(unsigned count)
{
    memset(&s, 0, sizeof(s));
    set_reg(&s, NVNET_TX_RING_PHYS_ADDR, 0x1000);
    set_reg(&s, NVNET_RING_SIZE, count - 1);
    set_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, 0x1000);
    set_reg(&s, NVNET_TRANSMITTER_CONTROL, NVNET_TRANSMITTER_CONTROL_START);
    set_reg(&s, NVNET_IRQ_MASK, NVNET_IRQ_STATUS_TX);
    s.phy_regs[MII_BMSR] = MII_BMSR_LINK_ST;
    set_dma_idle(&s, true);
}
static void put(unsigned i, unsigned byte, unsigned len, bool last)
{
    struct RingDesc d = { 0x90000 + i * 16, len - 1,
                          NV_TX_VALID | (last ? NV_TX_LASTPACKET : 0) };
    assert(d.buffer_addr + len <= sizeof(s.memory));
    memset(s.memory + d.buffer_addr, byte, len);
    memcpy(s.memory + 0x1000 + i * sizeof(d), &d, sizeof(d));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *c = argv[1];
    fresh(4);
    if (!strcmp(c, "mac-widths")) {
        const unsigned widths[] = { 1, 2, 4 };
        for (unsigned w = 0; w < 3; w++)
            for (unsigned offset = 0; offset < 8; offset += widths[w]) {
                memset(s.regs, 0xa5, sizeof(s.regs));
                nvnet_mmio_write(&s, NVNET_MAC_ADDR_A + offset, 0x12345678,
                                 widths[w]);
                for (unsigned j = 0; j < MMIO_SIZE; j++) {
                    uint8_t expected = 0xa5;
                    if (j >= NVNET_MAC_ADDR_A + offset &&
                        j < NVNET_MAC_ADDR_A + offset + widths[w])
                        expected = 0x12345678u >>
                                   (8 * (j - NVNET_MAC_ADDR_A - offset));
                    assert(s.regs[j] == expected);
                }
                assert(
                    nvnet_mmio_read(&s, NVNET_MAC_ADDR_A + offset, widths[w]) ==
                    (0x12345678u &
                     (widths[w] == 4 ? 0xffffffffu :
                                       ((1u << (8 * widths[w])) - 1))));
            }
        assert(s.irq_updates == 0 && s.loads == 0 && s.mdio_reads == 0 &&
               s.mdio_writes == 0);
    } else if (!strcmp(c, "w1c")) {
        set_reg(&s, NVNET_IRQ_STATUS, 0xffffffff);
        nvnet_mmio_write(&s, NVNET_IRQ_STATUS, 0x10, 1);
        assert(get_reg(&s, NVNET_IRQ_STATUS) == 0xffffffef &&
               s.irq_updates == 1);
        set_reg(&s, NVNET_MII_STATUS, 0xffffffff);
        nvnet_mmio_write(&s, NVNET_MII_STATUS, 0x1234, 2);
        assert(get_reg(&s, NVNET_MII_STATUS) == 0xffffedcb &&
               s.irq_updates == 2);
    } else if (!strcmp(c, "mask")) {
        set_reg(&s, NVNET_IRQ_STATUS, NVNET_IRQ_STATUS_TX);
        nvnet_mmio_write(&s, NVNET_IRQ_MASK, 0, 4);
        assert(!s.irq);
        nvnet_mmio_write(&s, NVNET_IRQ_MASK, NVNET_IRQ_STATUS_TX, 1);
        assert(s.irq);
    } else if (!strcmp(c, "control")) {
        put(0, 5, 1, true);
        nvnet_mmio_write(&s, NVNET_TX_RX_CONTROL, NVNET_TX_RX_CONTROL_KICK, 4);
        assert(s.sent == 1 && s.irq);
        s.tx_dma_buf_offset = 5;
        nvnet_mmio_write(&s, NVNET_TX_RX_CONTROL, NVNET_TX_RX_CONTROL_RESET, 4);
        assert(s.tx_dma_buf_offset == 0 &&
               get_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR) == 0x1000);
    } else if (!strcmp(c, "mdio")) {
        nvnet_mmio_write(&s, NVNET_MDIO_ADDR, 0, 4);
        nvnet_mmio_write(&s, NVNET_MDIO_ADDR, NVNET_MDIO_ADDR_WRITE, 4);
        assert(s.mdio_reads == 1 && s.mdio_writes == 1);
    } else if (!strcmp(c, "nonmac-lane")) {
        set_reg(&s, NVNET_IRQ_STATUS, 0xffffffff);
        nvnet_mmio_write(&s, NVNET_IRQ_STATUS + 1, 0x12, 1);
        return 2;
    } else if (!strcmp(c, "unaligned-mac")) {
        nvnet_mmio_write(&s, NVNET_MAC_ADDR_A + 1, 0xffff, 2);
        return 2;
    } else
        abort();
    puts("PASS");
    return 0;
}
