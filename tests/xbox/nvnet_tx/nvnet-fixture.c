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
static struct RingDesc peek(unsigned i)
{
    struct RingDesc d;
    memcpy(&d, s.memory + 0x1000 + i * sizeof(d), sizeof(d));
    return d;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *c = argv[1];
    fresh(4);
    if (!strcmp(c, "batch")) {
        put(0, 11, 3, true);
        put(1, 22, 5, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 2 && s.lengths[0] == 3 && s.lengths[1] == 5);
        assert(s.packets[0][2] == 11 && s.packets[1][4] == 22 && s.irq);
        assert(get_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR) == 0x1010);
        assert(!(peek(0).flags & NV_TX_VALID) &&
               !(peek(1).flags & NV_TX_VALID));
        assert(s.loads == 5 && s.writes == 2 && s.irq_updates == 1);
        assert(get_reg(&s, NVNET_TX_RX_CONTROL) & NVNET_TX_RX_CONTROL_IDLE);
    } else if (!strcmp(c, "wrap")) {
        set_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, 0x1018);
        put(3, 31, 2, false);
        put(0, 32, 3, true);
        put(1, 33, 4, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 2 && s.lengths[0] == 5 && s.lengths[1] == 4);
        assert(s.packets[0][0] == 31 && s.packets[0][2] == 32 &&
               s.packets[1][0] == 33);
        assert(get_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR) == 0x1010);
    } else if (!strcmp(c, "full")) {
        for (unsigned i = 0; i < 4; i++)
            put(i, i, 1, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 4 && s.loads == 8 && s.writes == 4);
        assert(get_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR) == 0x1000 &&
               s.irq_updates == 1);
    } else if (!strcmp(c, "split")) {
        put(0, 7, 2, false);
        dma_packet_from_guest(&s);
        assert(s.sent == 0 && s.tx_dma_buf_offset == 2 && !s.irq);
        put(1, 8, 3, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 1 && s.lengths[0] == 5 && s.packets[0][2] == 8 &&
               s.tx_dma_buf_offset == 0);
    } else if (!strcmp(c, "invalid")) {
        put(1, 9, 2, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 0 && s.loads == 1 && s.writes == 0 && !s.irq);
        assert(peek(1).flags & NV_TX_VALID);
    } else if (!strcmp(c, "disabled")) {
        for (unsigned i = 0; i < 3; i++) {
            fresh(4);
            put(0, 1, 1, true);
            if (i == 0)
                set_reg(&s, NVNET_TRANSMITTER_CONTROL, 0);
            if (i == 1)
                set_reg(&s, NVNET_TX_RX_CONTROL, NVNET_TX_RX_CONTROL_BIT2);
            if (i == 2)
                s.phy_regs[MII_BMSR] = 0;
            dma_packet_from_guest(&s);
            assert(s.loads == 0 && s.sent == 0 && s.writes == 0);
        }
    } else if (!strcmp(c, "flags")) {
        put(0, 1, 1, true);
        struct RingDesc d = peek(0);
        d.flags = 0xffff;
        memcpy(s.memory + 0x1000, &d, sizeof(d));
        dma_packet_from_guest(&s);
        assert(peek(0).flags ==
               (uint16_t)(0xffff &
                          ~(NV_TX_VALID | NV_TX_RETRYERROR | NV_TX_DEFERRED |
                            NV_TX_CARRIERLOST | NV_TX_LATECOLLISION |
                            NV_TX_UNDERFLOW | NV_TX_ERROR)));
    } else if (!strcmp(c, "max-ring")) {
        fresh(65536);
        assert(get_tx_ring_size(&s) == 0);
        /* Existing uint16_t overflow: record it; the batching patch does not
         * fix it. */
        dma_packet_from_guest(&s);
        assert(!s.loads && !s.sent);
        fresh(65535);
        for (unsigned i = 0; i < 65535; i++) {
            struct RingDesc d = { 0x90000, 0, NV_TX_VALID | NV_TX_LASTPACKET };
            memcpy(s.memory + 0x1000 + i * sizeof(d), &d, sizeof(d));
        }
        dma_packet_from_guest(&s);
        assert(s.sent == 65535 && s.writes == 65535 && s.loads == 131070);
    } else if (!strcmp(c, "buffer-exact")) {
        put(0, 1, TX_ALLOC_BUFSIZE, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 1 && s.lengths[0] == TX_ALLOC_BUFSIZE);
    } else if (!strcmp(c, "oversized")) {
        put(0, 1, TX_ALLOC_BUFSIZE, false);
        put(1, 2, 1, true);
        dma_packet_from_guest(&s);
        return 2;
    } else if (!strcmp(c, "next-outside")) {
        set_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, 0x2000);
        put(0, 1, 1, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 1);
        fresh(4);
        set_reg(&s, NVNET_TX_RING_NEXT_DESC_PHYS_ADDR, 0xff8);
        put(0, 1, 1, true);
        dma_packet_from_guest(&s);
        assert(s.sent == 1);
    } else if (!strcmp(c, "length-ffff")) {
        put(0, 1, 1, true);
        struct RingDesc d = peek(0);
        d.length = 0xffff;
        memcpy(s.memory + 0x1000, &d, sizeof(d));
        dma_packet_from_guest(&s);
        /* Existing uint16_t truncation sends zero bytes. Do not call this
         * valid. */
        assert(s.sent == 1 && s.lengths[0] == 0);
    } else
        abort();
    puts("PASS");
    return 0;
}
