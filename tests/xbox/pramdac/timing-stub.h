/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "regs.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t hwaddr;
#define NV2A_CRYSTAL_FREQ 16666666
#define VGA_MIS_COLOR 1
#define nv2a_reg_log_read(a, b, c, d) ((void)(b), (void)(c), (void)(d))
#define nv2a_reg_log_write(a, b, c, d) ((void)(b), (void)(c), (void)(d))
#define qatomic_read(p) (*(p))
#define qatomic_set(p, v) (*(p) = (v))
typedef struct PGRAPHState {
    uint32_t regs_[0x2000];
    bool flush_pending, flush_complete, waiting_for_nop, waiting_for_flip;
    bool waiting_for_context_switch, vsh_carry_cache_dirty;
    float vsh_carry_fog[4];
} PGRAPHState;
typedef struct NV2AState {
    struct {
        bool halt;
        uint32_t regs[0x2000], pending_interrupts;
    } pfifo;
    PGRAPHState pgraph;
    struct {
        uint32_t regs[0x2000];
    } pvideo;
    struct {
        uint32_t start, pending_interrupts;
    } pcrtc;
    struct {
        uint32_t pending_interrupts;
    } pmc, ptimer;
    struct {
        uint8_t palette[256 * 3];
    } puserdac;
    struct {
        unsigned msr;
    } vga;
    struct {
        uint32_t core_clock_coeff; /* PRAMDAC_FIELDS */
    } pramdac;
    bool locked;
} NV2AState;
static void nv2a_lock_fifo(NV2AState *d)
{
    assert(!d->locked);
    d->locked = true;
}
static void nv2a_unlock_fifo(NV2AState *d)
{
    assert(d->locked);
    d->locked = false;
}
static void qemu_event_reset(bool *p)
{
    *p = false;
}
static void qemu_event_wait(bool *p)
{
    *p = true;
}
static void bql_unlock(void)
{
}
static void bql_lock(void)
{
}
static void vga_common_reset(void *p)
{
    (void)p;
}
static void ptimer_reset(NV2AState *d)
{
    (void)d;
}
