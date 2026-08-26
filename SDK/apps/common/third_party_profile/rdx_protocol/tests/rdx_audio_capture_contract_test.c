#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rdx/rdx_capability.h"
#include "rdx_audio_capture_fake.h"

struct capture_observer {
    const uint8_t *data;
    uint32_t len;
    uint32_t engine_generation;
    uint32_t session_id;
    uint32_t capture_generation;
    enum rdx_record_cause fault;
    unsigned int frame_count;
    unsigned int fault_count;
};

static int capture_frame(void *priv,
                         uint32_t engine_generation,
                         uint32_t session_id,
                         uint32_t capture_generation,
                         const uint8_t *data,
                         uint32_t len)
{
    struct capture_observer *observer = priv;

    observer->data = data;
    observer->len = len;
    observer->engine_generation = engine_generation;
    observer->session_id = session_id;
    observer->capture_generation = capture_generation;
    observer->frame_count++;
    return 0;
}

static void capture_fault(void *priv, enum rdx_record_cause cause)
{
    struct capture_observer *observer = priv;

    observer->fault = cause;
    observer->fault_count++;
}

static void test_audio_capture_lifecycle(void)
{
    static const uint8_t first_frame[] = { 1, 2, 3 };
    static const uint8_t resumed_frame[] = { 4, 5 };
    const struct rdx_audio_capture_fake_state *fake;
    struct rdx_audio_capture_open_params params;
    struct capture_observer observer;

    memset(&observer, 0, sizeof(observer));
    memset(&params, 0, sizeof(params));
    params.scene = RDX_RECORD_SCENE_CALL;
    params.format_id = RDX_RECORD_FORMAT_CALL_V1;
    params.engine_generation = 3;
    params.session_id = 7;
    params.capture_generation = 11;
    params.frame_callback = capture_frame;
    params.fault_callback = capture_fault;
    params.frame_priv = &observer;

    rdx_audio_capture_fake_reset();
    assert(rdx_audio_capture_check_structure(params.scene, params.format_id)
           == RDX_AUDIO_CAPTURE_OK);
    assert(rdx_audio_capture_check_readiness(params.scene, params.format_id)
           == RDX_AUDIO_CAPTURE_OK);
    assert(rdx_audio_capture_check_structure(
               params.scene, RDX_RECORD_FORMAT_MEETING_V1)
           == RDX_AUDIO_CAPTURE_UNSUPPORTED);
    assert(rdx_audio_capture_open(NULL) == RDX_AUDIO_CAPTURE_UNSUPPORTED);
    assert(rdx_audio_capture_open(&params) == RDX_AUDIO_CAPTURE_OK);
    assert(rdx_audio_capture_open(&params) == RDX_AUDIO_CAPTURE_BUSY);

    assert(rdx_audio_capture_fake_emit_frame(first_frame,
                                             sizeof(first_frame)) == 0);
    assert(observer.data == first_frame);
    assert(observer.len == sizeof(first_frame));
    assert(observer.engine_generation == 3);
    assert(observer.session_id == 7);
    assert(observer.capture_generation == 11);
    assert(rdx_audio_capture_fake_emit_fault(RDX_RECORD_CAUSE_SOURCE_LOST)
           == 0);
    assert(observer.fault == RDX_RECORD_CAUSE_SOURCE_LOST);

    rdx_audio_capture_pause(params.scene);
    fake = rdx_audio_capture_fake_get_state();
    assert(!fake->active && fake->paused);
    assert(rdx_audio_capture_fake_emit_frame(first_frame,
                                             sizeof(first_frame)) != 0);

    params.capture_generation = 12;
    assert(rdx_audio_capture_resume(&params) == RDX_AUDIO_CAPTURE_OK);
    assert(rdx_audio_capture_fake_emit_frame(resumed_frame,
                                             sizeof(resumed_frame)) == 0);
    assert(observer.data == resumed_frame);
    assert(observer.capture_generation == 12);
    assert(observer.frame_count == 2);
    assert(observer.fault_count == 1);

    rdx_audio_capture_close(params.scene);
    rdx_audio_capture_close(params.scene);
    fake = rdx_audio_capture_fake_get_state();
    assert(!fake->active && !fake->paused);
    assert(fake->open_count == 1);
    assert(fake->pause_count == 1);
    assert(fake->resume_count == 1);
    assert(fake->close_count == 1);
}

static void test_audio_capture_failures(void)
{
    rdx_audio_capture_fake_reset();
    rdx_audio_capture_fake_set_structure_result(
        RDX_AUDIO_CAPTURE_FAILED);
    rdx_audio_capture_fake_set_readiness_result(
        RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE);
    assert(rdx_audio_capture_check_structure(
               RDX_RECORD_SCENE_MEETING,
               RDX_RECORD_FORMAT_MEETING_V1)
           == RDX_AUDIO_CAPTURE_FAILED);
    assert(rdx_audio_capture_check_readiness(
               RDX_RECORD_SCENE_MEETING,
               RDX_RECORD_FORMAT_MEETING_V1)
           == RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE);
}

static void test_capability_mapping(void)
{
    unsigned long caps;

    caps = RDX_REQUIRED_CAPS_FROM_ABILITY(RDX_ABILITY_NONE);
    assert(caps == RDX_REQUIRED_BASE_CAPS);
    caps = RDX_REQUIRED_CAPS_FROM_ABILITY(
        RDX_ABILITY_RTC
        | RDX_ABILITY_CONFERENCE_RECORDING
        | RDX_ABILITY_CALL_RECORDING);
    assert((caps & RDX_CAP_RTC) != 0);
    assert((caps & RDX_CAP_MEETING_CAPTURE) != 0);
    assert((caps & RDX_CAP_CALL_CAPTURE) != 0);
    assert((caps & RDX_REQUIRED_BASE_CAPS) == RDX_REQUIRED_BASE_CAPS);
    assert(RDX_CAP_RTC != RDX_ABILITY_RTC);
}

int main(void)
{
    test_audio_capture_lifecycle();
    test_audio_capture_failures();
    test_capability_mapping();
    puts("RDX audio capture contract tests passed");
    return 0;
}
