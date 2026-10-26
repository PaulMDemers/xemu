/* Projection test inserted into the current QEMU test-vmstate translation unit.
 * Actual field macros, real VMState serializer; NOT the complete NV2A schema.
 */
typedef struct NV2AState {
    struct {
        uint32_t core_clock_coeff;
        uint64_t core_clock_freq;
        uint32_t memory_clock_coeff, video_clock_coeff;
        uint32_t fp_vtotal, fp_vsync_start, fp_htotal;
    } pramdac;
} NV2AState;
static int timing_pre_load(void *opaque)
{
    NV2AState *d = opaque;
    d->pramdac.fp_vtotal = 0;
    d->pramdac.fp_vsync_start = 0;
    d->pramdac.fp_htotal = 0;
    return 0;
}
static const VMStateDescription timing_projection = {
    .name = "nv2a-pramdac-projection",
    .version_id = 6,
    .minimum_version_id = 1,
    .pre_load = timing_pre_load,
    .fields =
        (const VMStateField[]){ /* ACTUAL_FIELDS */
                                VMSTATE_END_OF_LIST() }
};
static void test_timing_projection(void)
{
    NV2AState sent = { .pramdac = { 0x12345678, 0x0123456789abcdefULL,
                                    0xabcdef01, 0xdeadbeef, 1124, 1083,
                                    2199 } };
    const uint8_t old_wire[] = {
        0x12, 0x34, 0x56, 0x78, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab,       0xcd,
        0xef, 0xab, 0xcd, 0xef, 0x01, 0xde, 0xad, 0xbe, 0xef, QEMU_VM_EOF
    };
    for (int v = 1; v <= 5; v++) {
        NV2AState received;
        memset(&received, 0xa5, sizeof(received));
        SUCCESS(load_vmstate_one(&timing_projection, &received, v, old_wire,
                                 sizeof(old_wire)));
        g_assert_cmphex(received.pramdac.core_clock_coeff, ==,
                        sent.pramdac.core_clock_coeff);
        g_assert_cmphex(received.pramdac.video_clock_coeff, ==,
                        sent.pramdac.video_clock_coeff);
        g_assert_cmpuint(received.pramdac.fp_vtotal, ==, 0);
        g_assert_cmpuint(received.pramdac.fp_vsync_start, ==, 0);
        g_assert_cmpuint(received.pramdac.fp_htotal, ==, 0);
    }
    save_vmstate(&timing_projection, &sent);
    QEMUFile *f = open_test_file(false);
    NV2AState received;
    memset(&received, 0xa5, sizeof(received));
    Error *err = NULL;
    SUCCESS(vmstate_load_state(f, &timing_projection, &received, 6, &err));
    g_assert_null(err);
    g_assert_cmphex(received.pramdac.core_clock_freq, ==,
                    sent.pramdac.core_clock_freq);
    g_assert_cmpuint(received.pramdac.fp_vtotal, ==, 1124);
    g_assert_cmpuint(received.pramdac.fp_vsync_start, ==, 1083);
    g_assert_cmpuint(received.pramdac.fp_htotal, ==, 2199);
    g_assert_cmpuint(qemu_get_byte(f), ==, QEMU_VM_EOF);
    qemu_fclose(f);
}
