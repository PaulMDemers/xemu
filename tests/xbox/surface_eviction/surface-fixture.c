/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t hwaddr;
typedef int MemoryRegion;
#define PRIM_TYPE_INVALID 0
#define trace_nv2a_pgraph_surface_cpu_write(a, b) ((void)(a), (void)(b))
#define trace_nv2a_pgraph_surface_cpu_read(a, b) ((void)(a), (void)(b))
#define trace_nv2a_pgraph_surface_evict_reason(a, b) ((void)(a), (void)(b))
#define qatomic_set(p, v) (*(p) = (v))
typedef struct SurfaceBinding {
    hwaddr vram_addr, size;
    bool draw_dirty, download_pending, upload_pending, cpu_write_evict_pending,
        invalid;
    struct SurfaceBinding *next;
    unsigned downloads;
} SurfaceBinding;
typedef struct PGRAPHGLState {
    SurfaceBinding *surfaces, *color_binding, *zeta_binding;
    bool downloads_pending, downloads_complete;
} PGRAPHGLState;
typedef struct SurfaceState {
    bool buffer_dirty, draw_dirty, write_enabled_cache;
} SurfaceState;
typedef struct PGRAPHState {
    int lock, primitive_mode;
    PGRAPHGLState *gl_renderer_state;
    SurfaceState surface_color, surface_zeta;
} PGRAPHState;
typedef struct NV2AState {
    PGRAPHState pgraph;
    struct {
        int lock;
    } pfifo;
    uint8_t ram[128], gpu[128];
} NV2AState;
#define QTAILQ_FOREACH(v, h, e) for ((v) = *(h); (v); (v) = (v)->next)
#define QTAILQ_FOREACH_SAFE(v, h, e, n) \
    for ((v) = *(h); (v) && ((n) = (v)->next, 1); (v) = (n))
static bool cpu_self, begin_race;
static unsigned invalidations, waits;
static void qemu_mutex_lock(int *p)
{
    assert(*p == 0);
    *p = 1;
}
static void qemu_mutex_unlock(int *p)
{
    assert(*p == 1);
    *p = 0;
}
static int qemu_get_cpu(int i)
{
    assert(i == 0);
    return 0;
}
static bool qemu_cpu_is_self(int c)
{
    assert(c == 0);
    return cpu_self;
}
static void qemu_event_reset(bool *p)
{
    *p = false;
}
static void qemu_event_set(bool *p)
{
    *p = true;
}
static void qemu_event_wait(bool *p)
{
    ++waits;
    assert(*p);
}
static void surface_download(NV2AState *d, SurfaceBinding *s, bool force)
{
    assert(!s->invalid);
    if (s->draw_dirty && (force || s->download_pending)) {
        assert(s->vram_addr + s->size <= sizeof(d->ram));
        memcpy(d->ram + s->vram_addr, d->gpu + s->vram_addr, s->size);
        s->draw_dirty = false;
        s->download_pending = false;
        ++s->downloads;
    }
}
static void pgraph_gl_surface_download_if_dirty(NV2AState *d, SurfaceBinding *s)
{
    surface_download(d, s, true);
}
static void pgraph_gl_surface_invalidate(NV2AState *d, SurfaceBinding *s)
{
    assert(!s->draw_dirty && !s->invalid);
    PGRAPHGLState *r = d->pgraph.gl_renderer_state;
    SurfaceBinding **p = &r->surfaces;
    while (*p != s) {
        assert(*p);
        p = &(*p)->next;
    }
    *p = s->next;
    s->invalid = true;
    ++invalidations;
    if (r->color_binding == s)
        r->color_binding = NULL;
    if (r->zeta_binding == s)
        r->zeta_binding = NULL;
}
void pgraph_gl_process_pending_downloads(NV2AState *d);
static void pfifo_kick(NV2AState *d)
{
    if (begin_race)
        d->pgraph.primitive_mode = 1;
    qemu_mutex_lock(&d->pgraph.lock);
    pgraph_gl_process_pending_downloads(d);
    qemu_mutex_unlock(&d->pgraph.lock);
}
/* ACTUAL_FUNCTIONS */

static NV2AState d;
static PGRAPHGLState r;
static SurfaceBinding a, b;
static void fresh(void)
{
    memset(&d, 0, sizeof(d));
    memset(&r, 0, sizeof(r));
    a = (SurfaceBinding){ .vram_addr = 16, .size = 32, .draw_dirty = true };
    b = (SurfaceBinding){ .vram_addr = 32, .size = 32, .draw_dirty = true };
    d.pgraph.gl_renderer_state = &r;
    r.surfaces = &a;
    r.color_binding = &a;
    d.pgraph.surface_color = (SurfaceState){ false, true, true };
    d.pgraph.surface_zeta = (SurfaceState){ false, true, true };
    for (unsigned i = 0; i < 128; i++)
        d.gpu[i] = i ^ 0xa5;
    cpu_self = true;
    begin_race = false;
    invalidations = waits = 0;
}
static void store(unsigned addr)
{
    surface_access_callback(&d, NULL, addr, 1, true);
    d.ram[addr] = 0x42;
}
int main(void)
{
    fresh();
    store(20);
    assert(a.invalid && invalidations == 1 && a.downloads == 1 && waits == 1);
    for (unsigned i = 16; i < 48; i++)
        assert(d.ram[i] == (i == 20 ? 0x42 : d.gpu[i]));
    assert(d.pgraph.surface_color.buffer_dirty &&
           !d.pgraph.surface_color.draw_dirty &&
           !d.pgraph.surface_color.write_enabled_cache);
    puts("PASS: GPU contents preserved before CPU store and eviction");
    fresh();
    d.pgraph.primitive_mode = 1;
    store(20);
    assert(!a.invalid && a.upload_pending && !a.cpu_write_evict_pending &&
           a.downloads == 1);
    assert(r.color_binding == &a);
    puts("PASS: open primitive retains binding");
    fresh();
    d.pgraph.primitive_mode = 1;
    a.draw_dirty = false;
    store(20);
    assert(!a.invalid && waits == 0 && !a.cpu_write_evict_pending);
    puts("PASS: clean open primitive does not request eviction or wait");
    fresh();
    begin_race = true;
    store(20);
    assert(!a.invalid && !a.cpu_write_evict_pending && a.downloads == 1);
    d.pgraph.primitive_mode = 0;
    pgraph_gl_process_pending_downloads(&d);
    assert(!a.invalid);
    puts("PASS: BEGIN between request and worker clears deferred eviction");
    fresh();
    cpu_self = false;
    store(20);
    assert(!a.invalid && !a.cpu_write_evict_pending && a.downloads == 1);
    puts("PASS: non-vCPU caller retains ownership policy");
    fresh();
    surface_access_callback(&d, NULL, 20, 1, false);
    assert(!a.invalid && !a.upload_pending && a.downloads == 1);
    puts("PASS: CPU read downloads without eviction");
    fresh();
    a.draw_dirty = false;
    store(20);
    assert(a.invalid && a.downloads == 0 && waits == 1);
    puts("PASS: clean surface eviction still waits for worker");
    fresh();
    a.next = &b;
    r.zeta_binding = &b;
    store(40);
    assert(a.invalid && b.invalid && !r.surfaces && invalidations == 2);
    assert(a.downloads == 1 && b.downloads == 1 &&
           d.pgraph.surface_zeta.buffer_dirty);
    puts("PASS: overlapping modeled bindings survive safe list removal");
    fresh();
    store(48);
    assert(!a.invalid && !waits && !a.upload_pending);
    fresh();
    store(15);
    assert(!a.invalid && !waits && !a.upload_pending);
    puts("PASS: non-overlap and exact range boundaries");
    return 0;
}
