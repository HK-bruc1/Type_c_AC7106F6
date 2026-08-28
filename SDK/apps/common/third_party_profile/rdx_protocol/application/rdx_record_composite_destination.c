#include <string.h>

#include "rdx/rdx_record_composite_destination.h"

static int composite_attach(void *priv, uint32_t session_id,
                            uint32_t capture_generation)
{
    struct rdx_record_composite_destination *composite = priv;
    int ret;

    if (!composite || composite->local_attached || composite->online_attached
        || !composite->local_ops
        || !composite->local_ops->attach) {
        return -1;
    }
    composite->session_id = session_id;
    composite->capture_generation = capture_generation;
    ret = composite->local_ops->attach(
        composite->local_priv, session_id, capture_generation);
    if (ret) {
        return ret;
    }
    composite->local_attached = 1;

    /* Online is optional.  A failed attach only removes the mirror. */
    composite->online_attached = 0;
    if (composite->online_ops && composite->online_ops->attach
        && !composite->online_ops->attach(
               composite->online_priv, session_id, capture_generation)) {
        composite->online_attached = 1;
    }
    return 0;
}

static int composite_consume(void *priv,
                             const struct rdx_record_frame *frame)
{
    struct rdx_record_composite_destination *composite = priv;
    int ret;

    if (!composite || !composite->local_attached || !frame
        || frame->session_id != composite->session_id
        || frame->capture_generation != composite->capture_generation
        || !composite->local_ops || !composite->local_ops->consume) {
        return -1;
    }
    ret = composite->local_ops->consume(composite->local_priv, frame);
    if (ret) {
        /* Local REQUIRED failure must reach Engine and terminate the
         * session; do not let Online make it look like a successful frame. */
        return ret;
    }
    if (composite->online_attached && composite->online_ops
        && composite->online_ops->consume
        && composite->online_ops->consume(composite->online_priv, frame)) {
        if (composite->online_ops->abort) {
            composite->online_ops->abort(
                composite->online_priv, composite->session_id,
                composite->capture_generation, RDX_RECORD_CAUSE_LINK_LOST);
        } else if (composite->online_ops->cancel) {
            composite->online_ops->cancel(
                composite->online_priv, composite->session_id,
                composite->capture_generation);
        }
        composite->online_attached = 0;
    }
    return 0;
}

static void composite_poll(void *priv, uint32_t session_id,
                           uint32_t capture_generation)
{
    struct rdx_record_composite_destination *composite = priv;

    if (!composite || !composite->online_attached
        || session_id != composite->session_id
        || capture_generation != composite->capture_generation
        || !composite->online_ops || !composite->online_ops->poll) {
        return;
    }
    composite->online_ops->poll(composite->online_priv,
                                session_id, capture_generation);
}

static int composite_seal(void *priv, uint32_t session_id,
                          uint32_t capture_generation)
{
    struct rdx_record_composite_destination *composite = priv;
    int ret;

    if (!composite || !composite->local_attached
        || session_id != composite->session_id
        || capture_generation != composite->capture_generation
        || !composite->local_ops || !composite->local_ops->seal) {
        return -1;
    }
    ret = composite->local_ops->seal(composite->local_priv,
                                     session_id, capture_generation);
    if (ret) {
        return ret;
    }
    if (composite->online_attached && composite->online_ops
        && composite->online_ops->seal) {
        /* Online completion is optional; Local commit remains authoritative. */
        composite->online_ops->seal(composite->online_priv,
                                    session_id, capture_generation);
    }
    return 0;
}

static int composite_abort(void *priv, uint32_t session_id,
                           uint32_t capture_generation,
                           enum rdx_record_cause cause)
{
    struct rdx_record_composite_destination *composite = priv;
    int ret = 0;

    if (!composite) {
        return -1;
    }

    /* START rollback may happen before attach() is called, or after local
     * attach has failed.  The Local destination owns the prepared artifact
     * in both cases and its abort implementation distinguishes the unbound
     * cancel_start path from a token-bound abort.  Do not gate that cleanup
     * on local_attached, otherwise a failed capture readiness/attach leaves
     * a temp artifact owned by nobody. */
    if (composite->local_ops && composite->local_ops->abort) {
        if ((composite->local_attached || composite->session_id
             || composite->capture_generation)
            && (session_id != composite->session_id
                || capture_generation != composite->capture_generation)) {
            return -1;
        }
        ret = composite->local_ops->abort(composite->local_priv,
                                          session_id, capture_generation,
                                          cause);
    }
    if (composite->online_attached && composite->online_ops
        && session_id == composite->session_id
        && capture_generation == composite->capture_generation) {
        if (composite->online_ops->abort) {
            composite->online_ops->abort(composite->online_priv,
                                         session_id, capture_generation,
                                         cause);
        } else if (composite->online_ops->cancel) {
            composite->online_ops->cancel(composite->online_priv,
                                          session_id, capture_generation);
        }
    }
    return ret;
}

static void composite_detach_capture(void *priv, uint32_t session_id,
                                     uint32_t capture_generation)
{
    struct rdx_record_composite_destination *composite = priv;

    if (!composite || session_id != composite->session_id
        || capture_generation != composite->capture_generation) {
        return;
    }
    if (composite->local_attached && composite->local_ops) {
        if (composite->local_ops->detach_capture) {
            composite->local_ops->detach_capture(
                composite->local_priv, session_id, capture_generation);
        } else if (composite->local_ops->detach) {
            composite->local_ops->detach(
                composite->local_priv, session_id, capture_generation);
        }
    }
    if (composite->online_attached && composite->online_ops) {
        if (composite->online_ops->detach_capture) {
            composite->online_ops->detach_capture(
                composite->online_priv, session_id, capture_generation);
        } else if (composite->online_ops->detach) {
            composite->online_ops->detach(
                composite->online_priv, session_id, capture_generation);
        }
    }
    composite->local_attached = 0;
    composite->online_attached = 0;
}

static void composite_cancel(void *priv, uint32_t session_id,
                             uint32_t capture_generation)
{
    struct rdx_record_composite_destination *composite = priv;

    if (!composite || session_id != composite->session_id
        || capture_generation != composite->capture_generation) {
        return;
    }
    if (composite->online_attached && composite->online_ops
        && composite->online_ops->cancel) {
        composite->online_ops->cancel(composite->online_priv,
                                      session_id, capture_generation);
    }
}

static void composite_detach(void *priv, uint32_t session_id,
                             uint32_t capture_generation)
{
    composite_detach_capture(priv, session_id, capture_generation);
}

static const struct rdx_record_destination_ops composite_ops = {
    .attach = composite_attach,
    .consume = composite_consume,
    .poll = composite_poll,
    .seal = composite_seal,
    .abort = composite_abort,
    .cancel = composite_cancel,
    .detach = composite_detach,
    .detach_capture = composite_detach_capture,
    .requirement = RDX_RECORD_DESTINATION_REQUIRED,
};

int rdx_record_composite_destination_init(
    struct rdx_record_composite_destination *composite,
    const struct rdx_record_destination_ops *local_ops, void *local_priv,
    const struct rdx_record_destination_ops *online_ops, void *online_priv)
{
    if (!composite || !local_ops || !local_ops->attach
        || !local_ops->consume || !local_ops->seal || !local_ops->abort) {
        return -1;
    }
    memset(composite, 0, sizeof(*composite));
    composite->local_ops = local_ops;
    composite->local_priv = local_priv;
    composite->online_ops = online_ops;
    composite->online_priv = online_priv;
    return 0;
}

const struct rdx_record_destination_ops *
rdx_record_composite_destination_ops(void)
{
    return &composite_ops;
}
