/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pramdac.c"
#include "reset-load.h"
static NV2AState d;
static unsigned checks;
#define CHECK(x)   \
    do {           \
        ++checks;  \
        assert(x); \
    } while (0)
int main(int argc, char **argv)
{
    assert(argc == 2);
    const uint32_t offsets[] = { 0x804, 0x80c, 0x824, 0x800, 0x808,
                                 0x810, 0x818, 0x820, 0x828, 0x838 };
    uint32_t values[10] = { 0 };
    if (!strcmp(argv[1], "readback")) {
        for (unsigned n = 0; n < 4096; n++)
            for (unsigned i = 0; i < 10; i++) {
                uint32_t v = (n * 0x9e3779b9u) ^ (i * 0x1234567u);
                pramdac_write(&d, offsets[i], v, 4);
                values[i] = v;
                for (unsigned j = 0; j < 10; j++)
                    CHECK(pramdac_read(&d, offsets[j], 4) == values[j]);
            }
        pramdac_write(&d, 0x8f0, 0xffffffffu, 4);
        CHECK(pramdac_read(&d, 0x8f0, 4) == 0);
    } else if (!strcmp(argv[1], "reset")) {
        for (unsigned cycle = 0; cycle < 4; cycle++) {
            for (unsigned i = 0; i < 3; i++)
                pramdac_write(&d, offsets[i], 0xffffffffu - cycle, 4);
            d.pfifo.halt = cycle & 1;
            nv2a_reset(&d);
            for (unsigned i = 0; i < 3; i++)
                CHECK(pramdac_read(&d, offsets[i], 4) == 0);
            CHECK(d.pfifo.halt == (bool)(cycle & 1) && !d.locked);
            CHECK(d.pgraph.vsh_carry_fog[3] == 1.0f);
        }
    } else if (!strcmp(argv[1], "preload")) {
        for (unsigned version = 1; version <= 6; version++) {
            for (unsigned i = 0; i < 3; i++)
                pramdac_write(&d, offsets[i], 0xffffffffu, 4);
            CHECK(nv2a_pre_load(&d) == 0 && d.locked);
            for (unsigned i = 0; i < 3; i++)
                CHECK(pramdac_read(&d, offsets[i], 4) == 0);
            CHECK(nv2a_post_load(&d, version) == 0 && !d.locked);
        }
    } else
        return 2;
    printf("PASS: %u checks\n", checks);
    return 0;
}
