#include <assert.h>
#include <string.h>

#include "rdx/rdx_record_composite_destination.h"

struct fake_destination {
    int attach_count;
    int consume_count;
    int seal_count;
    int abort_count;
    int detach_count;
    int fail_attach;
    int fail_consume;
};

static int fake_attach(void *priv, uint32_t session_id,
                       uint32_t capture_generation)
{
    struct fake_destination *fake = priv;
    (void)session_id;
    (void)capture_generation;
    fake->attach_count++;
    return fake->fail_attach;
}

static int fake_consume(void *priv, const struct rdx_record_frame *frame)
{
    struct fake_destination *fake = priv;
    (void)frame;
    fake->consume_count++;
    return fake->fail_consume;
}

static int fake_seal(void *priv, uint32_t session_id,
                     uint32_t capture_generation)
{
    struct fake_destination *fake = priv;
    (void)session_id;
    (void)capture_generation;
    fake->seal_count++;
    return 0;
}

static int fake_abort(void *priv, uint32_t session_id,
                      uint32_t capture_generation,
                      enum rdx_record_cause cause)
{
    struct fake_destination *fake = priv;
    (void)session_id;
    (void)capture_generation;
    (void)cause;
    fake->abort_count++;
    return 0;
}

static void fake_detach(void *priv, uint32_t session_id,
                        uint32_t capture_generation)
{
    struct fake_destination *fake = priv;
    (void)session_id;
    (void)capture_generation;
    fake->detach_count++;
}

static const struct rdx_record_destination_ops fake_ops = {
    .attach = fake_attach,
    .consume = fake_consume,
    .seal = fake_seal,
    .abort = fake_abort,
    .detach = fake_detach,
};

static struct rdx_record_frame test_frame(void)
{
    struct rdx_record_frame frame;
    static const uint8_t payload[2] = { 0x11, 0x22 };

    memset(&frame, 0, sizeof(frame));
    frame.session_id = 9;
    frame.capture_generation = 3;
    frame.payload = payload;
    frame.payload_len = sizeof(payload);
    return frame;
}

static void test_local_required_online_optional(void)
{
    struct fake_destination local = { 0 };
    struct fake_destination online = { 0 };
    struct rdx_record_composite_destination composite;
    const struct rdx_record_destination_ops *ops;
    struct rdx_record_frame frame = test_frame();

    assert(rdx_record_composite_destination_init(
               &composite, &fake_ops, &local, &fake_ops, &online) == 0);
    ops = rdx_record_composite_destination_ops();
    assert(ops->requirement == RDX_RECORD_DESTINATION_REQUIRED);
    assert(ops->attach(&composite, 9, 3) == 0);
    assert(ops->attach(&composite, 9, 3) != 0);
    assert(local.attach_count == 1 && online.attach_count == 1);
    assert(ops->consume(&composite, &frame) == 0);
    assert(local.consume_count == 1 && online.consume_count == 1);
    assert(ops->seal(&composite, 9, 3) == 0);
    assert(local.seal_count == 1 && online.seal_count == 1);
    ops->detach_capture(&composite, 9, 3);
    assert(local.detach_count == 1 && online.detach_count == 1);
}

static void test_local_failure_does_not_mirror(void)
{
    struct fake_destination local = { 0 };
    struct fake_destination online = { 0 };
    struct rdx_record_composite_destination composite;
    const struct rdx_record_destination_ops *ops;
    struct rdx_record_frame frame = test_frame();

    local.fail_consume = -1;
    assert(rdx_record_composite_destination_init(
               &composite, &fake_ops, &local, &fake_ops, &online) == 0);
    ops = rdx_record_composite_destination_ops();
    assert(ops->attach(&composite, 9, 3) == 0);
    assert(ops->consume(&composite, &frame) != 0);
    assert(local.consume_count == 1 && online.consume_count == 0);
    assert(ops->abort(&composite, 9, 3, RDX_RECORD_CAUSE_QUEUE_FULL) == 0);
    assert(local.abort_count == 1 && online.abort_count == 1);
}

static void test_abort_before_attach_cleans_local(void)
{
    struct fake_destination local = { 0 };
    struct fake_destination online = { 0 };
    struct rdx_record_composite_destination composite;
    const struct rdx_record_destination_ops *ops;

    assert(rdx_record_composite_destination_init(
               &composite, &fake_ops, &local, &fake_ops, &online) == 0);
    ops = rdx_record_composite_destination_ops();
    /* Engine START can fail before it reaches destination->attach(). */
    assert(ops->abort(&composite, 9, 3,
                      RDX_RECORD_CAUSE_SOURCE_LOST) == 0);
    assert(local.abort_count == 1);
    assert(online.abort_count == 0);
}

int main(void)
{
    test_local_required_online_optional();
    test_local_failure_does_not_mirror();
    test_abort_before_attach_cleans_local();
    return 0;
}
