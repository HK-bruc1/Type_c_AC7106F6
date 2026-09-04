#include <stddef.h>
#if !CONFIG_CPU_BR56
#include <stdio.h>
#endif
#include <string.h>

#if CONFIG_CPU_BR56
#include "app_config.h"
#endif
#include "rdx/rdx_local_storage_service.h"
#include "rdx_storage_command_slot.h"

#if CONFIG_CPU_BR56
#include "system/includes.h"
#include "system/timer.h"
#include "task.h"
#include "user_cfg.h"
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE && TCFG_RESOURCE_MONITOR_ENABLE
#include "resource_monitor.h"
#define service_diag_failure(reason)                 \
    do {                                             \
        resource_monitor_snapshot((reason));         \
        resource_monitor_dump_key_tasks((reason));  \
    } while (0)
#else
#define service_diag_failure(reason)
#endif
#define RDX_LOCAL_STORAGE_TASK_NAME       "rdx_storage"
#define RDX_LOCAL_STORAGE_TASK_WAKE       0x52445802
#define RDX_LOCAL_STORAGE_PUMP_BUDGET     RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES
#endif

enum service_command_type {
    SERVICE_COMMAND_NONE = 0,
    SERVICE_COMMAND_RECOVER,
    SERVICE_COMMAND_RESERVE_ARTIFACT,
    SERVICE_COMMAND_START,
    SERVICE_COMMAND_ATTACH,
    SERVICE_COMMAND_CANCEL_START,
    SERVICE_COMMAND_SEAL,
    SERVICE_COMMAND_ABORT,
    SERVICE_COMMAND_GET_CAPACITY,
    SERVICE_COMMAND_ENUMERATE,
    SERVICE_COMMAND_DELETE,
    SERVICE_COMMAND_FORMAT,
    SERVICE_COMMAND_QUIESCE,
    SERVICE_COMMAND_SHUTDOWN,
};

/* Every payload stored in the target command slot is worker-owned.  Caller
 * output pointers are kept only on the waiting task's stack and are copied
 * after a matching completion is claimed, so a timeout cannot leave the
 * worker referencing an expired caller object. */
struct service_command {
    enum service_command_type type;
    enum rdx_storage_result result;
    uint32_t token;
    union {
        struct {
            enum rdx_record_format_id format_id;
            struct rdx_storage_artifact_spec spec;
        } reserve;
        struct {
            struct rdx_storage_artifact_spec spec;
        } start;
        struct {
            uint32_t session_id;
            uint32_t capture_generation;
        } attach;
        struct {
            enum rdx_record_cause cause;
        } cancel_start;
        struct {
            uint32_t session_id;
            uint32_t capture_generation;
        } seal;
        struct {
            uint32_t session_id;
            uint32_t capture_generation;
            enum rdx_record_cause cause;
        } abort;
        struct {
            uint64_t total_bytes;
            uint64_t free_bytes;
        } capacity;
        struct {
            uint32_t capacity;
            uint32_t count;
        } enumerate;
        struct {
            struct rdx_storage_file_id file_id;
        } delete_file;
        struct {
            uint16_t timeout_ms;
        } quiesce;
        struct {
            uint16_t timeout_ms;
        } shutdown;
    } data;
};

struct service_command_output {
    struct rdx_storage_artifact_spec *reserved_spec;
    uint64_t *total_bytes;
    uint64_t *free_bytes;
    struct rdx_storage_file_info *items;
    uint32_t capacity;
    uint32_t *count;
};

static enum rdx_storage_result service_recover_executor(
    struct rdx_local_storage_service *service);
static enum rdx_storage_result service_reserve_artifact_executor(
    struct rdx_local_storage_service *service,
    enum rdx_record_format_id format_id,
    struct rdx_storage_artifact_spec *spec);
static enum rdx_storage_result service_start_executor(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_artifact_spec *spec);
static enum rdx_storage_result service_attach_executor(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation);
static enum rdx_storage_result service_cancel_start_executor(
    struct rdx_local_storage_service *service, enum rdx_record_cause cause);
static enum rdx_storage_result service_seal_executor(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation);
static enum rdx_storage_result service_abort_executor(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation, enum rdx_record_cause cause);
static enum rdx_storage_result service_get_capacity_executor(
    struct rdx_local_storage_service *service, uint64_t *total_bytes,
    uint64_t *free_bytes);
static enum rdx_storage_result service_enumerate_executor(
    struct rdx_local_storage_service *service,
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count);
static enum rdx_storage_result service_delete_executor(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_file_id *file_id);
static enum rdx_storage_result service_format_executor(
    struct rdx_local_storage_service *service);
static enum rdx_storage_result service_quiesce_executor(
    struct rdx_local_storage_service *service, uint16_t timeout_ms);
static enum rdx_storage_result service_shutdown_executor(
    struct rdx_local_storage_service *service, uint16_t timeout_ms);
static enum rdx_storage_result service_upload_close_executor(
    struct rdx_local_storage_service *service,
    uint32_t connection_generation, uint32_t session_token,
    uint8_t force);
static void service_process_upload_close(
    struct rdx_local_storage_service *service);

#if CONFIG_CPU_BR56

struct rdx_local_storage_platform_context {
    struct rdx_local_storage_service *owner;
    OS_MUTEX executor_lock;
    OS_SEM command_done;
    spinlock_t state_lock;
    struct service_command command;
    struct rdx_storage_command_slot command_slot;
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
    uint32_t command_enqueue_ms;
    uint32_t command_run_start_ms;
    uint32_t command_wait_max_ms;
    uint32_t command_run_max_ms;
    uint32_t wake_post_ms;
    uint32_t wake_posts;
    uint32_t wake_coalesced;
    uint32_t wake_failures;
    uint32_t wake_latency_max_ms;
    uint32_t frame_batches;
    uint32_t command_timeouts;
    uint32_t pending_cancellations;
    uint32_t pending_must_run;
    uint32_t late_completions;
    uint8_t queue_peak;
    uint8_t wake_latency_pending;
#endif
};

/* The product runtime owns exactly one Local Storage Service.  Keeping the
 * JL synchronization objects here makes that target-only ownership explicit
 * without leaking OS_MUTEX through the public RDX header. */
static struct rdx_local_storage_platform_context
    rdx_local_storage_platform_context;
#endif

#ifndef service_diag_failure
#define service_diag_failure(reason)
#endif

#define RDX_COMPAT_FILENAME_ATTEMPTS      64

#if CONFIG_CPU_BR56 && RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
static void service_diag_reset_locked(
    struct rdx_local_storage_platform_context *context)
{
    uint32_t next_token = context->command_slot.next_token;

    memset(&context->command_enqueue_ms, 0,
           sizeof(*context)
               - offsetof(struct rdx_local_storage_platform_context,
                          command_enqueue_ms));
    context->command_slot.next_token = next_token;
}

static void service_diag_max(uint32_t *maximum, uint32_t value)
{
    if (value > *maximum) {
        *maximum = value;
    }
}

static void service_diag_command_enqueued_locked(
    struct rdx_local_storage_platform_context *context,
    enum service_command_type type)
{
    if (type == SERVICE_COMMAND_RESERVE_ARTIFACT) {
        service_diag_reset_locked(context);
    }
    context->command_enqueue_ms = sys_timer_get_ms();
}

static void service_diag_command_started_locked(
    struct rdx_local_storage_platform_context *context)
{
    uint32_t now = sys_timer_get_ms();

    context->command_run_start_ms = now;
    service_diag_max(&context->command_wait_max_ms,
                     now - context->command_enqueue_ms);
}

static void service_diag_command_finished_locked(
    struct rdx_local_storage_platform_context *context)
{
    service_diag_max(&context->command_run_max_ms,
                     sys_timer_get_ms() - context->command_run_start_ms);
}

static void service_diag_dump(
    struct rdx_local_storage_platform_context *context, const char *reason)
{
    printf("[RDX][STORAGE_EXEC] reason=%s wake_posts=%u"
           " wake_coalesced=%u wake_fail=%u wake_latency_max_ms=%u"
           " frame_batches=%u queue_peak=%u command_wait_max_ms=%u"
           " command_run_max_ms=%u timeouts=%u pending_cancel=%u"
           " pending_must_run=%u late_completion=%u\n",
           reason ? reason : "unknown",
           (unsigned int)context->wake_posts,
           (unsigned int)context->wake_coalesced,
           (unsigned int)context->wake_failures,
           (unsigned int)context->wake_latency_max_ms,
           (unsigned int)context->frame_batches,
           (unsigned int)context->queue_peak,
           (unsigned int)context->command_wait_max_ms,
           (unsigned int)context->command_run_max_ms,
           (unsigned int)context->command_timeouts,
           (unsigned int)context->pending_cancellations,
           (unsigned int)context->pending_must_run,
           (unsigned int)context->late_completions);
#if TCFG_RESOURCE_MONITOR_ENABLE
    resource_monitor_snapshot(reason);
    resource_monitor_dump_key_tasks(reason);
#endif
}

#define service_diag_queue_peak_locked(context, count)                 \
    do {                                                                \
        if ((count) > (context)->queue_peak) {                          \
            (context)->queue_peak = (count);                            \
        }                                                               \
    } while (0)
#else
#define service_diag_command_enqueued_locked(context, type)
#define service_diag_command_started_locked(context)
#define service_diag_command_finished_locked(context)
#define service_diag_dump(context, reason)
#define service_diag_queue_peak_locked(context, count)
#endif

/* CRC32/IEEE is part of the Native Artifact checkpoint contract.  Keep the
 * serialized value in the Artifact (rather than a backend-private running
 * state) so a checkpoint can be compared with the value returned by close().
 * The first frame uses the standard init/xorout pair; subsequent frames feed
 * the previous finalized value back into the running state. */
static uint32_t service_crc32_update(uint32_t previous,
                                     uint32_t frame_count,
                                     const uint8_t *data, uint32_t len)
{
    uint32_t crc = frame_count ? (previous ^ 0xffffffffUL) : 0xffffffffUL;
    uint32_t i;

    for (i = 0; i < len; i++) {
        uint32_t bit;

        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1)
                  ^ (0xedb88320UL & (uint32_t)-(int)(crc & 1));
        }
    }
    return crc ^ 0xffffffffUL;
}

#if CONFIG_CPU_BR56
static struct rdx_local_storage_platform_context *service_platform_context(
    struct rdx_local_storage_service *service)
{
    if (!service || !service->platform_initialized
        || !service->platform_context
        || ((struct rdx_local_storage_platform_context *)
                service->platform_context)->owner != service) {
        return NULL;
    }
    return service->platform_context;
}

static void service_executor_lock(struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_platform_context *context =
        service_platform_context(service);

    if (context) {
        os_mutex_pend(&context->executor_lock, 0);
    }
}

static void service_executor_unlock(struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_platform_context *context =
        service_platform_context(service);

    if (context) {
        os_mutex_post(&context->executor_lock);
    }
}

static void service_state_lock(struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_platform_context *context =
        service_platform_context(service);

    if (context) {
        spin_lock(&context->state_lock);
    }
}

static void service_state_unlock(struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_platform_context *context =
        service_platform_context(service);

    if (context) {
        spin_unlock(&context->state_lock);
    }
}

static void service_wake(struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_platform_context *context;
    uint8_t post = 0;
    int ret;

    service_state_lock(service);
    context = service_platform_context(service);
    if (service && service->worker_created && !service->worker_stop
        && !service->worker_wake_pending) {
        service->worker_wake_pending = 1;
        post = 1;
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
        if (context) {
            context->wake_posts++;
            context->wake_post_ms = sys_timer_get_ms();
            context->wake_latency_pending = 1;
        }
#endif
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
    } else if (context && service && service->worker_created
               && !service->worker_stop && service->worker_wake_pending) {
        context->wake_coalesced++;
#endif
    }
    service_state_unlock(service);
    if (!post) {
        return;
    }
    ret = os_taskq_post_msg(RDX_LOCAL_STORAGE_TASK_NAME, 1,
                            RDX_LOCAL_STORAGE_TASK_WAKE);
    if (ret) {
        service_state_lock(service);
        service->worker_wake_pending = 0;
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
        if (context) {
            context->wake_failures++;
            context->wake_latency_pending = 0;
        }
#endif
        service_state_unlock(service);
    }
}
#else
static void service_executor_lock(struct rdx_local_storage_service *service)
{
    (void)service;
}

static void service_executor_unlock(struct rdx_local_storage_service *service)
{
    (void)service;
}

static void service_state_lock(struct rdx_local_storage_service *service)
{
    (void)service;
}

static void service_state_unlock(struct rdx_local_storage_service *service)
{
    (void)service;
}

static void service_wake(struct rdx_local_storage_service *service)
{
    (void)service;
}
#endif

static uint8_t service_command_busy_locked(
    struct rdx_local_storage_service *service)
{
#if CONFIG_CPU_BR56
    struct rdx_local_storage_platform_context *context =
        service_platform_context(service);

    return context
           && rdx_storage_command_slot_busy(&context->command_slot);
#else
    (void)service;
    return 0;
#endif
}

static enum rdx_storage_result service_execute_command(
    struct rdx_local_storage_service *service,
    struct service_command *command)
{
    enum rdx_storage_result close_result;

    /* A synchronous lifecycle/media command supersedes an idle upload
     * session.  Closing here preserves the single FAT owner and avoids a
     * persistent reader blocking recording, format, quiesce or shutdown. */
    close_result = service_upload_close_executor(service, 0, 0, 1);
    if (close_result != RDX_STORAGE_RESULT_READY) {
        return close_result;
    }
    switch (command->type) {
    case SERVICE_COMMAND_RECOVER:
        return service_recover_executor(service);
    case SERVICE_COMMAND_RESERVE_ARTIFACT:
        return service_reserve_artifact_executor(
            service, command->data.reserve.format_id,
            &command->data.reserve.spec);
    case SERVICE_COMMAND_START:
        return service_start_executor(service, &command->data.start.spec);
    case SERVICE_COMMAND_ATTACH:
        return service_attach_executor(
            service, command->data.attach.session_id,
            command->data.attach.capture_generation);
    case SERVICE_COMMAND_CANCEL_START:
        return service_cancel_start_executor(
            service, command->data.cancel_start.cause);
    case SERVICE_COMMAND_SEAL:
        return service_seal_executor(
            service, command->data.seal.session_id,
            command->data.seal.capture_generation);
    case SERVICE_COMMAND_ABORT:
        return service_abort_executor(
            service, command->data.abort.session_id,
            command->data.abort.capture_generation,
            command->data.abort.cause);
    case SERVICE_COMMAND_GET_CAPACITY:
        return service_get_capacity_executor(
            service, &command->data.capacity.total_bytes,
            &command->data.capacity.free_bytes);
    case SERVICE_COMMAND_ENUMERATE:
        {
            uint32_t capacity = command->data.enumerate.capacity;
            const uint32_t scratch_capacity =
                sizeof(service->scratch.file_items)
                    / sizeof(service->scratch.file_items[0]);

            if (capacity > scratch_capacity) {
                capacity = scratch_capacity;
            }
            return service_enumerate_executor(
                service, capacity ? service->scratch.file_items : NULL,
                capacity, &command->data.enumerate.count);
        }
    case SERVICE_COMMAND_DELETE:
        return service_delete_executor(
            service, &command->data.delete_file.file_id);
    case SERVICE_COMMAND_FORMAT:
        return service_format_executor(service);
    case SERVICE_COMMAND_QUIESCE:
        return service_quiesce_executor(
            service, command->data.quiesce.timeout_ms);
    case SERVICE_COMMAND_SHUTDOWN:
        return service_shutdown_executor(
            service, command->data.shutdown.timeout_ms);
    default:
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
}

static enum rdx_storage_result service_copy_command_output(
    struct rdx_local_storage_service *service,
    const struct service_command *command,
    const struct service_command_output *output)
{
    if (!output) {
        return command->result;
    }
    switch (command->type) {
    case SERVICE_COMMAND_RESERVE_ARTIFACT:
        if (output->reserved_spec) {
            *output->reserved_spec = command->data.reserve.spec;
        }
        break;
    case SERVICE_COMMAND_GET_CAPACITY:
        if (output->total_bytes) {
            *output->total_bytes = command->data.capacity.total_bytes;
        }
        if (output->free_bytes) {
            *output->free_bytes = command->data.capacity.free_bytes;
        }
        break;
    case SERVICE_COMMAND_ENUMERATE:
        if (!output->count) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        *output->count = command->data.enumerate.count;
        if (command->result != RDX_STORAGE_RESULT_READY) {
            break;
        }
        if (command->data.enumerate.count > output->capacity
            || (command->data.enumerate.count && !output->items)) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        if (command->data.enumerate.count) {
            memcpy(output->items, service->scratch.file_items,
                   command->data.enumerate.count
                       * sizeof(service->scratch.file_items[0]));
        }
        break;
    default:
        break;
    }
    return command->result;
}

#if CONFIG_CPU_BR56
static uint32_t service_command_timeout_ms(
    const struct service_command *command)
{
    switch (command->type) {
    case SERVICE_COMMAND_SEAL:
    case SERVICE_COMMAND_FORMAT:
        return RDX_CFG_LOCAL_STORAGE_LONG_COMMAND_TIMEOUT_MS;
    default:
        return RDX_CFG_LOCAL_STORAGE_COMMAND_TIMEOUT_MS;
    }
}

static uint8_t service_command_pending_cancellable(
    const struct service_command *command)
{
    switch (command->type) {
    case SERVICE_COMMAND_CANCEL_START:
    case SERVICE_COMMAND_SEAL:
    case SERVICE_COMMAND_ABORT:
    case SERVICE_COMMAND_SHUTDOWN:
        /* Once accepted, lifecycle cleanup and commit barriers must run even
         * if their synchronous caller no longer waits for completion. */
        return 0;
    default:
        return 1;
    }
}

static void service_cancel_pending_command_locked(
    struct rdx_local_storage_service *service,
    struct rdx_local_storage_platform_context *context,
    const struct service_command *command, uint8_t latch_seal)
{
    if (latch_seal && service->active && service->seal_requested
        && service->session_id == command->data.seal.session_id
        && service->capture_generation
               == command->data.seal.capture_generation) {
        service->seal_requested = 0;
    }
    memset(&context->command, 0, sizeof(context->command));
}

static void service_reconcile_late_command(
    struct rdx_local_storage_service *service,
    const struct service_command *command)
{
    if (command->result != RDX_STORAGE_RESULT_READY) {
        return;
    }
    if (command->type == SERVICE_COMMAND_START) {
        /* A caller that timed out cannot attach this prepared Artifact. */
        (void)service_cancel_start_executor(
            service, RDX_RECORD_CAUSE_STORAGE_ERROR);
    } else if (command->type == SERVICE_COMMAND_ATTACH) {
        /* Engine START cannot safely retain an attachment whose completion
         * was not observed by its coordinator. */
        (void)service_abort_executor(
            service, command->data.attach.session_id,
            command->data.attach.capture_generation,
            RDX_RECORD_CAUSE_STORAGE_ERROR);
    }
}
#endif

static enum rdx_storage_result service_run_command_internal(
    struct rdx_local_storage_service *service,
    struct service_command *command, uint8_t latch_seal,
    const struct service_command_output *output)
{
    enum rdx_storage_result result;

    if (!service || !command || command->type == SERVICE_COMMAND_NONE) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
#if CONFIG_CPU_BR56
    {
        struct rdx_local_storage_platform_context *context =
            service_platform_context(service);
        enum rdx_storage_command_deadline_result deadline_result;
        uint32_t wait_start_ms;
        uint32_t timeout_ms;
        uint32_t token;
        int wait_result;

        if (!context || !service->worker_created || service->worker_stop) {
            return RDX_STORAGE_RESULT_BAD_STATE;
        }
        if (cpu_in_irq()) {
            return RDX_STORAGE_RESULT_BAD_STATE;
        }
        if (os_current_task()
            && !strcmp(os_current_task(), RDX_LOCAL_STORAGE_TASK_NAME)) {
            if (latch_seal) {
                service_state_lock(service);
                if (!service->active || service->seal_requested
                    || service->session_id
                           != command->data.seal.session_id
                    || service->capture_generation
                           != command->data.seal.capture_generation) {
                    service_state_unlock(service);
                    return RDX_STORAGE_RESULT_BAD_STATE;
                }
                service->seal_requested = 1;
                service_state_unlock(service);
            }
            service_executor_lock(service);
            result = service_execute_command(service, command);
            service_executor_unlock(service);
            command->result = result;
            return service_copy_command_output(
                service, command, output);
        }

        service_state_lock(service);
        if (service_command_busy_locked(service)
            || service->operation_pending || service->operation_running
            || service->upload_close_pending) {
            service_state_unlock(service);
            return RDX_STORAGE_RESULT_BUSY;
        }
        if (latch_seal) {
            if (!service->active || service->seal_requested
                || service->session_id != command->data.seal.session_id
                || service->capture_generation
                       != command->data.seal.capture_generation) {
                service_state_unlock(service);
                return RDX_STORAGE_RESULT_BAD_STATE;
            }
            service->seal_requested = 1;
        }
        token = rdx_storage_command_slot_submit(&context->command_slot);
        if (!token) {
            service_state_unlock(service);
            return RDX_STORAGE_RESULT_BUSY;
        }
        command->token = token;
        context->command = *command;
        service_diag_command_enqueued_locked(context, command->type);
        service_state_unlock(service);
        service_wake(service);

        timeout_ms = service_command_timeout_ms(command);
        wait_start_ms = sys_timer_get_ms();
        for (;;) {
            uint32_t elapsed_ms = sys_timer_get_ms() - wait_start_ms;
            uint32_t remaining_ms = elapsed_ms < timeout_ms
                                    ? timeout_ms - elapsed_ms : 0;
            uint32_t timeout_ticks = remaining_ms
                ? msecs_to_jiffies(remaining_ms - 1) + 1 : 1;

            wait_result = os_sem_pend(&context->command_done,
                                      (int)timeout_ticks);
            service_state_lock(service);
            if (rdx_storage_command_slot_claim(
                    &context->command_slot, token)) {
                *command = context->command;
                service_state_unlock(service);
                result = service_copy_command_output(
                    service, command, output);
                service_state_lock(service);
                if (!rdx_storage_command_slot_release(
                        &context->command_slot, token)) {
                    result = RDX_STORAGE_RESULT_BAD_STATE;
                }
                memset(&context->command, 0, sizeof(context->command));
                service_state_unlock(service);
                return result;
            }
            elapsed_ms = sys_timer_get_ms() - wait_start_ms;
            if (wait_result == OS_NO_ERR && elapsed_ms < timeout_ms) {
                service_state_unlock(service);
                continue;
            }
            deadline_result = rdx_storage_command_slot_deadline(
                &context->command_slot, token,
                service_command_pending_cancellable(command));
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
            context->command_timeouts++;
            service_diag_max(&context->command_wait_max_ms, elapsed_ms);
#endif
            if (deadline_result
                == RDX_STORAGE_COMMAND_DEADLINE_CANCELLED_PENDING) {
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
                context->pending_cancellations++;
#endif
                service_cancel_pending_command_locked(
                    service, context, command, latch_seal);
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
            } else if (deadline_result
                       == RDX_STORAGE_COMMAND_DEADLINE_ABANDONED_PENDING) {
                context->pending_must_run++;
#endif
            }
            service_state_unlock(service);
            printf("[RDX][STORAGE_EXEC] command_timeout type=%u token=%u"
                   " phase=%u wait_ms=%u os_ret=%d\n",
                   command->type, (unsigned int)token, deadline_result,
                   (unsigned int)elapsed_ms, wait_result);
            service_diag_failure("rdx_storage_command_timeout");
            return deadline_result
                       == RDX_STORAGE_COMMAND_DEADLINE_COMPLETED
                       ? RDX_STORAGE_RESULT_BAD_STATE
                       : RDX_STORAGE_RESULT_TIMEOUT;
        }
    }
#else
    if (latch_seal) {
        if (!service->active || service->seal_requested
            || service->session_id != command->data.seal.session_id
            || service->capture_generation
                   != command->data.seal.capture_generation) {
            return RDX_STORAGE_RESULT_BAD_STATE;
        }
        service->seal_requested = 1;
    }
    service_executor_lock(service);
    result = service_execute_command(service, command);
    service_executor_unlock(service);
    command->result = result;
    return service_copy_command_output(service, command, output);
#endif
}

static enum rdx_storage_result service_run_command(
    struct rdx_local_storage_service *service,
    struct service_command *command, uint8_t latch_seal)
{
    return service_run_command_internal(
        service, command, latch_seal, NULL);
}

#if CONFIG_CPU_BR56
static void service_process_command(
    struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_platform_context *context =
        service_platform_context(service);
    enum rdx_storage_command_complete_result complete_result;
    enum service_command_type type;
    uint32_t token;
    uint8_t post_completion = 0;

    if (!context) {
        return;
    }
    service_state_lock(service);
    token = context->command.token;
    if (!rdx_storage_command_slot_start(
            &context->command_slot, token)) {
        service_state_unlock(service);
        return;
    }
    type = context->command.type;
    service_diag_command_started_locked(context);
    service_state_unlock(service);

    service_executor_lock(service);
    context->command.result = service_execute_command(
        service, &context->command);
    service_state_lock(service);
    service_diag_command_finished_locked(context);
    complete_result = rdx_storage_command_slot_complete(
        &context->command_slot, token);
    if (complete_result == RDX_STORAGE_COMMAND_COMPLETE_READY) {
        post_completion = 1;
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
    } else if (complete_result
               == RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED) {
        context->late_completions++;
#endif
    }
    service_state_unlock(service);
    if (complete_result == RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED) {
        service_reconcile_late_command(service, &context->command);
    }
    service_executor_unlock(service);

    /* Dump while this token still owns the slot.  A following RESERVE resets
     * per-recording counters and must not race the terminal snapshot. */
    if (type == SERVICE_COMMAND_SEAL) {
        service_diag_dump(context, "storage_seal");
    } else if (type == SERVICE_COMMAND_ABORT) {
        service_diag_dump(context, "storage_abort");
    } else if (type == SERVICE_COMMAND_FORMAT) {
        service_diag_dump(context, "storage_format");
    }
    if (complete_result == RDX_STORAGE_COMMAND_COMPLETE_LATE_DROPPED) {
        printf("[RDX][STORAGE_EXEC] late_completion type=%u token=%u"
               " result=%u action=reconciled\n",
               type, (unsigned int)token, context->command.result);
        service_state_lock(service);
        (void)rdx_storage_command_slot_release(
            &context->command_slot, token);
        memset(&context->command, 0, sizeof(context->command));
        service_state_unlock(service);
    } else if (complete_result == RDX_STORAGE_COMMAND_COMPLETE_STALE) {
        printf("[RDX][STORAGE_EXEC] stale_completion type=%u token=%u\n",
               type, (unsigned int)token);
    }
    if (post_completion) {
        os_sem_post(&context->command_done);
    }
}
#endif

static uint8_t service_operation_busy(
    const struct rdx_local_storage_service *service)
{
    return service
           && (service->operation_pending || service->operation_running);
}

static enum rdx_storage_result service_upload_close_executor(
    struct rdx_local_storage_service *service,
    uint32_t connection_generation, uint32_t session_token,
    uint8_t force)
{
    struct rdx_storage_file_info file;
    enum rdx_storage_result result;
    void *reader;
    uint32_t reads;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    service_state_lock(service);
    if (!service->upload_reader
        || (!force
            && (service->upload_connection_generation
                    != connection_generation
                || service->upload_session_token != session_token))) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_READY;
    }
    reader = service->upload_reader;
    file = service->upload_file;
    reads = service->upload_session_read_count;
    service->upload_reader = NULL;
    memset(&service->upload_file, 0, sizeof(service->upload_file));
    service->upload_connection_generation = 0;
    service->upload_session_token = 0;
    service->upload_session_read_count = 0;
    service->upload_close_count++;
    service_state_unlock(service);

    result = service->media_ops && service->media_ops->close_reader
                 ? service->media_ops->close_reader(reader)
                 : RDX_STORAGE_RESULT_UNSUPPORTED;
    printf("[RDX][LOCAL_UPLOAD] close result=%u sn=%u reads=%u"
           " resolves=%u opens=%u closes=%u\n",
           result, (unsigned int)file.file_id.serial_number,
           (unsigned int)reads,
           (unsigned int)service->upload_identity_resolve_count,
           (unsigned int)service->upload_open_count,
           (unsigned int)service->upload_close_count);
    return result;
}

static void service_process_upload_close(
    struct rdx_local_storage_service *service)
{
    uint32_t connection_generation;
    uint32_t session_token;

    service_state_lock(service);
    if (!service || !service->upload_close_pending) {
        service_state_unlock(service);
        return;
    }
    connection_generation = service->upload_close_connection_generation;
    session_token = service->upload_close_session_token;
    service->upload_close_pending = 0;
    service->upload_close_connection_generation = 0;
    service->upload_close_session_token = 0;
    service_state_unlock(service);

    service_executor_lock(service);
    (void)service_upload_close_executor(
        service, connection_generation, session_token, 0);
    service_executor_unlock(service);
}

static int service_ids_match(const struct rdx_local_storage_service *service,
                             uint32_t session_id,
                             uint32_t capture_generation)
{
    return service && service->active && service->session_id == session_id
           && service->capture_generation == capture_generation;
}

static uint8_t service_native_batch_frame_limit(
    const struct rdx_local_storage_service *service)
{
    uint32_t limit;

    if (!service || !service->artifact.spec.payload_size) {
        return 1;
    }
    limit = RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_BYTES
            / service->artifact.spec.payload_size;
    if (!limit) {
        limit = 1;
    }
    if (limit > RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES) {
        limit = RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES;
    }
    return (uint8_t)limit;
}

static uint8_t service_write_ready_locked(
    const struct rdx_local_storage_service *service)
{
    if (!service || !service->active || !service->queue_count) {
        return 0;
    }
    if (service->seal_requested
        || service->backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        return 1;
    }
    return service->queue_count >= service_native_batch_frame_limit(service);
}

static void service_fill_marker(
    const struct rdx_local_storage_service *service,
    uint32_t marker_state, struct rdx_storage_marker *marker)
{
    const struct rdx_artifact *artifact = &service->artifact;

    memset(marker, 0, sizeof(*marker));
    marker->artifact_id = artifact->spec.artifact_id;
    marker->generation = artifact->generation;
    marker->state = marker_state;
    marker->session_id = artifact->spec.session_id;
    marker->serial_number = artifact->spec.serial_number;
    marker->produced_frame_count = artifact->produced_frame_count;
    marker->destination_accepted_count =
        artifact->destination_accepted_count;
    marker->local_accepted_count = artifact->local_accepted_count;
    marker->persisted_frame_count = artifact->persisted_frame_count;
    marker->rejected_frame_count = artifact->rejected_frame_count;
    marker->dropped_frame_count = artifact->dropped_frame_count;
    marker->first_error_cause = artifact->first_error_cause;
    marker->audio_bytes = artifact->audio_bytes;
    marker->audio_crc = artifact->audio_crc;
    memcpy(marker->temp_name, artifact->spec.temp_name,
           sizeof(marker->temp_name));
    memcpy(marker->final_name, artifact->spec.final_name,
           sizeof(marker->final_name));
}

static void service_emit(struct rdx_local_storage_service *service,
                         enum rdx_local_storage_completion_type type,
                         enum rdx_storage_result result,
                         enum rdx_record_cause cause)
{
    struct rdx_local_storage_completion completion;

    if (!service || service->completion_sent) {
        return;
    }
    memset(&completion, 0, sizeof(completion));
    completion.type = type;
    completion.result = result;
    completion.artifact_id = service->artifact.spec.artifact_id;
    completion.generation = service->artifact.generation;
    completion.serial_number = service->artifact.spec.serial_number;
    completion.frame_count = service->artifact.persisted_frame_count;
    completion.produced_frame_count =
        service->artifact.produced_frame_count;
    completion.destination_accepted_count =
        service->artifact.destination_accepted_count;
    completion.local_accepted_count =
        service->artifact.local_accepted_count;
    completion.persisted_frame_count =
        service->artifact.persisted_frame_count;
    completion.rejected_frame_count =
        service->artifact.rejected_frame_count;
    completion.dropped_frame_count =
        service->artifact.dropped_frame_count;
    completion.file_size = service->artifact.audio_bytes;
    completion.audio_crc = service->artifact.audio_crc;
    completion.cause = cause;
    service->completion_sent = 1;
    if (service->completion_callback) {
        service->completion_callback(service->completion_priv, &completion);
    }
}

/* A failure can happen after the backend has appended the new entry but
 * before the INDEXED marker is durable.  Best-effort removal keeps a stale
 * index entry from being advertised as a committed file.  If the media is
 * already unavailable, recovery will retry from the marker/index pair. */
static void service_remove_native_index_entry(
    struct rdx_local_storage_service *service)
{
    struct rdx_storage_index_snapshot snapshot;
    uint32_t i;

    if (!service || service->backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE
        || !service->native_ops || !service->native_ops->load_index
        || !service->native_ops->commit_index) {
        return;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = service->scratch.index_entries;
    snapshot.capacity = sizeof(service->scratch.index_entries)
                        / sizeof(service->scratch.index_entries[0]);
    if (service->native_ops->load_index(&snapshot)
        != RDX_STORAGE_RESULT_READY
        || snapshot.count > snapshot.capacity) {
        return;
    }
    for (i = 0; i < snapshot.count; i++) {
        if (snapshot.entries[i].file.file_id.artifact_id.value
            == service->artifact.spec.artifact_id.value) {
            if (i + 1 < snapshot.count) {
                memmove(&snapshot.entries[i], &snapshot.entries[i + 1],
                        (snapshot.count - i - 1)
                        * sizeof(snapshot.entries[0]));
            }
            snapshot.count--;
            if (service->native_ops->commit_index(&snapshot)
                == RDX_STORAGE_RESULT_READY
                && snapshot.generation != UINT32_MAX) {
                service_state_lock(service);
                service->index_generation = snapshot.generation + 1;
                service_state_unlock(service);
            }
            return;
        }
    }
}

static enum rdx_storage_result service_fail(
    struct rdx_local_storage_service *service,
    enum rdx_storage_result result, enum rdx_record_cause cause)
{
    struct rdx_storage_marker marker;
    void *artifact_handle = NULL;
    uint8_t native_backend;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }

    /* Reject new frames and release the bounded queue before touching FAT.
     * The executor lock serializes backend transactions; the state lock only
     * protects the admission snapshot and is never held across VFS calls. */
    service_state_lock(service);
    if (service->artifact.state != RDX_ARTIFACT_STATE_NONE
        && service->artifact.state != RDX_ARTIFACT_STATE_FAILED
        && service->artifact.state != RDX_ARTIFACT_STATE_QUARANTINED) {
        rdx_artifact_fail(&service->artifact, cause);
    }
    native_backend = service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
                     && service->native_ops;
    if (native_backend) {
        artifact_handle = service->artifact_handle;
        service->artifact_handle = NULL;
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_FAILED,
                            &marker);
    }
    service->queue_head = 0;
    service->queue_tail = 0;
    service->queue_count = 0;
    service->checkpoint_persisted_frame_count = 0;
    service->active = 0;
    service->capture_detached = 1;
    service_state_unlock(service);

    if (native_backend) {
        if (artifact_handle && service->native_ops->close) {
            uint64_t file_size = 0;
            uint32_t audio_crc = 0;

            service->native_ops->close(artifact_handle, &file_size,
                                       &audio_crc);
        }
        if (service->native_ops->store_marker) {
            service->native_ops->store_marker(&marker);
        }
        service_remove_native_index_entry(service);
        if (service->native_ops->quarantine) {
            enum rdx_storage_result quarantine_result;

            quarantine_result = service->native_ops->quarantine(
                &service->artifact.spec.artifact_id, "rdx-storage-failed");
            service_state_lock(service);
            if (quarantine_result == RDX_STORAGE_RESULT_READY
                && service->artifact.state == RDX_ARTIFACT_STATE_FAILED) {
                rdx_artifact_quarantine(&service->artifact);
            }
            service_state_unlock(service);
        }
    } else if (service->backend == RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE
               && service->legacy_ops && service->legacy_ops->abort_file) {
        service->legacy_ops->abort_file(&service->file_id);
    }
    service_state_lock(service);
    service->readiness =
        service->artifact.state == RDX_ARTIFACT_STATE_QUARANTINED
            ? RDX_STORAGE_READINESS_READY
            : RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
    service_state_unlock(service);
    service_emit(service,
                 service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
                     ? RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED
                     : RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_FAILED,
                 result, cause);
    return result;
}

static enum rdx_storage_result service_operation_enumerate_scratch(
    struct rdx_local_storage_service *service, uint32_t *count,
    uint32_t *generation)
{
    uint32_t capacity = sizeof(service->scratch.file_items)
                        / sizeof(service->scratch.file_items[0]);
    enum rdx_storage_result result;
    uint32_t current_generation = 0;

    memset(service->scratch.file_items, 0,
           sizeof(service->scratch.file_items));
    result = service->media_ops->enumerate(
        service->scratch.file_items, capacity, count,
        &current_generation);
    if (result == RDX_STORAGE_RESULT_READY) {
        service_state_lock(service);
        service->index_generation = current_generation;
        service_state_unlock(service);
        if (generation) {
            *generation = current_generation;
        }
    }
    return result;
}

static struct rdx_storage_file_info *service_operation_find_file(
    struct rdx_local_storage_service *service, uint32_t serial_number,
    const char *filename, enum rdx_storage_result *result,
    uint32_t *generation)
{
    struct rdx_storage_file_info *match = NULL;
    uint32_t count = 0;
    uint32_t i;

    *result = service_operation_enumerate_scratch(
        service, &count, generation);
    if (*result != RDX_STORAGE_RESULT_READY) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        struct rdx_storage_file_info *item =
            &service->scratch.file_items[i];

        if (item->file_id.serial_number != serial_number) {
            continue;
        }
        if (filename
            && strncmp(item->file_id.name, filename,
                       sizeof(item->file_id.name))) {
            continue;
        }
        if (match) {
            *result = RDX_STORAGE_RESULT_CORRUPT;
            return NULL;
        }
        match = item;
    }
    if (!match) {
        *result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    return match;
}

static enum rdx_storage_result service_operation_upload_read(
    struct rdx_local_storage_service *service,
    const struct rdx_local_storage_operation_request *request,
    struct rdx_local_storage_operation_completion *completion)
{
    const uint32_t requested_length = request->data.read.length;
    enum rdx_storage_result result = RDX_STORAGE_RESULT_READY;
    struct rdx_storage_file_info *file = NULL;
    uint32_t expected_length;
    void *reader = NULL;

    if (!service->media_ops->open_reader
        || !service->media_ops->read_reader
        || !service->media_ops->close_reader) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (request->data.read.open_session) {
        result = service_upload_close_executor(service, 0, 0, 1);
        if (result != RDX_STORAGE_RESULT_READY) {
            return result;
        }
        service->upload_identity_resolve_count++;
        file = service_operation_find_file(
            service, request->data.read.serial_number, NULL,
            &result, &completion->storage_generation);
        if (!file) {
            return result;
        }
        completion->file = *file;
        if (request->data.read.offset >= file->file_size) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        result = service->media_ops->open_reader(
            file, &reader);
        if (result != RDX_STORAGE_RESULT_READY
            || !reader) {
            return result == RDX_STORAGE_RESULT_READY
                       ? RDX_STORAGE_RESULT_IO_ERROR : result;
        }
        service_state_lock(service);
        service->upload_reader = reader;
        service->upload_file = *file;
        service->upload_connection_generation =
            request->data.read.connection_generation;
        service->upload_session_token =
            request->data.read.session_token;
        service->upload_open_count++;
        service->upload_session_read_count = 0;
        service_state_unlock(service);
    } else {
        if (!service->upload_reader
            || service->upload_connection_generation
                   != request->data.read.connection_generation
            || service->upload_session_token
                   != request->data.read.session_token
            || service->upload_file.file_id.serial_number
                   != request->data.read.serial_number) {
            return RDX_STORAGE_RESULT_BAD_STATE;
        }
        completion->file = service->upload_file;
        completion->storage_generation = service->index_generation;
    }

    if (request->data.read.offset >= service->upload_file.file_size) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    expected_length = (uint32_t)(
        service->upload_file.file_size - request->data.read.offset);
    if (expected_length > requested_length) {
        expected_length = requested_length;
    }
    result = service->media_ops->read_reader(
        service->upload_reader, request->data.read.offset,
        request->data.read.buffer, requested_length,
        &completion->read_length);
    if (result == RDX_STORAGE_RESULT_READY
        && completion->read_length != expected_length) {
        result = RDX_STORAGE_RESULT_IO_ERROR;
    }
    if (result == RDX_STORAGE_RESULT_READY) {
        service->upload_read_count++;
        service->upload_session_read_count++;
        completion->upload_session_token =
            service->upload_session_token;
        completion->file = service->upload_file;
        return result;
    }
    (void)service_upload_close_executor(service, 0, 0, 1);
    return result;
}

static void service_operation_update_readiness(
    struct rdx_local_storage_service *service,
    enum rdx_storage_result result)
{
    if (result == RDX_STORAGE_RESULT_READY
        || result == RDX_STORAGE_RESULT_BAD_ARGUMENT
        || result == RDX_STORAGE_RESULT_NO_SPACE) {
        return;
    }
    if (result == RDX_STORAGE_RESULT_NOT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service->recovery_checked = 0;
    } else if (result == RDX_STORAGE_RESULT_CORRUPT
               || result == RDX_STORAGE_RESULT_CHECKSUM) {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
    }
}

static enum rdx_storage_result service_format_and_recover(
    struct rdx_local_storage_service *service,
    uint64_t *total_bytes, uint64_t *free_bytes)
{
    enum rdx_storage_result result;

    if (!service || !service->media_ops || !service->media_ops->format
        || !service->media_ops->ensure_ready) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service->readiness = RDX_STORAGE_READINESS_RECOVERING;
    service->recovery_checked = 0;
    result = service->media_ops->format();
    if (result == RDX_STORAGE_RESULT_READY) {
        result = service->media_ops->ensure_ready();
    }
    if (result == RDX_STORAGE_RESULT_READY
        && service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops && service->native_ops->recover) {
        result = service->native_ops->recover();
    }
    if (result == RDX_STORAGE_RESULT_READY
        && total_bytes && free_bytes && service->media_ops->get_capacity) {
        result = service->media_ops->get_capacity(
            total_bytes, free_bytes);
    }
    if (result == RDX_STORAGE_RESULT_READY
        && service->media_ops->enumerate) {
        uint32_t count = 0;
        uint32_t generation = 0;

        result = service_operation_enumerate_scratch(
            service, &count, &generation);
        if (result == RDX_STORAGE_RESULT_READY && count) {
            result = RDX_STORAGE_RESULT_CORRUPT;
        }
    }
    if (result == RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_READY;
        service->recovery_checked = 1;
    } else if (result == RDX_STORAGE_RESULT_NOT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
    } else {
        /* A failed or only partially completed format cannot leave the
         * Service indefinitely in RECOVERING.  Gate recording until an
         * explicit bounded recovery proves the new media state. */
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
    }
    return result;
}

static void service_process_operation(
    struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_operation_request request;
    struct rdx_local_storage_operation_completion completion;
    rdx_local_storage_operation_callback_t callback;
    void *callback_priv;

    memset(&completion, 0, sizeof(completion));
    service_state_lock(service);
    if (!service || !service->operation_pending
        || service->operation_running) {
        service_state_unlock(service);
        return;
    }
    request = service->operation;
    callback = service->operation_callback;
    callback_priv = service->operation_priv;
    service->operation_pending = 0;
    service->operation_running = 1;
    service_state_unlock(service);

    completion.type = request.type;
    completion.token = request.token;
    completion.result = RDX_STORAGE_RESULT_UNSUPPORTED;
    service_executor_lock(service);
    if (request.type != RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ) {
        completion.result = service_upload_close_executor(
            service, 0, 0, 1);
        if (completion.result != RDX_STORAGE_RESULT_READY) {
            goto operation_complete;
        }
    }
    switch (request.type) {
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
        if (service->media_ops->get_capacity) {
            completion.result = service->media_ops->get_capacity(
                &completion.total_bytes, &completion.free_bytes);
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        if (service->media_ops->enumerate) {
            completion.result = service->media_ops->enumerate(
                request.data.enumerate.items,
                request.data.enumerate.capacity, &completion.count,
                &completion.storage_generation);
            if (completion.result == RDX_STORAGE_RESULT_READY) {
                service_state_lock(service);
                service->index_generation =
                    completion.storage_generation;
                service_state_unlock(service);
            }
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ:
        if (service->media_ops->enumerate) {
            completion.result = service_operation_upload_read(
                service, &request, &completion);
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY:
        if (service->media_ops->enumerate && service->media_ops->delete) {
            enum rdx_storage_result find_result;
            struct rdx_storage_file_info *file;

            file = service_operation_find_file(
                service, request.data.delete_file.serial_number,
                request.data.delete_file.filename, &find_result,
                &completion.storage_generation);
            completion.result = find_result;
            if (file) {
                completion.file = *file;
                completion.result = service->media_ops->delete(
                    &file->file_id);
                if (completion.result == RDX_STORAGE_RESULT_READY
                    && completion.storage_generation != UINT32_MAX) {
                    completion.storage_generation++;
                    service_state_lock(service);
                    service->index_generation =
                        completion.storage_generation;
                    service_state_unlock(service);
                }
            }
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        if (service->media_ops->format) {
            completion.result = service_format_and_recover(
                service, &completion.total_bytes, &completion.free_bytes);
        }
        break;
    default:
        completion.result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
        break;
    }
operation_complete:
    service_executor_unlock(service);

    service_state_lock(service);
    service_operation_update_readiness(service, completion.result);
    service->operation_running = 0;
    memset(&service->operation, 0, sizeof(service->operation));
    service->operation_callback = NULL;
    service->operation_priv = NULL;
    service_state_unlock(service);
    if (callback) {
        callback(callback_priv, &completion);
    }
}

#if CONFIG_CPU_BR56
static void rdx_local_storage_service_task(void *priv)
{
    struct rdx_local_storage_service *service = priv;
    int msg[4];

    if (!service) {
        return;
    }
    while (!service->worker_stop) {
        struct rdx_local_storage_platform_context *context;
        uint8_t has_command;
        uint8_t has_operation;
        uint8_t has_upload_close;
        uint8_t has_work;

        service_state_lock(service);
        context = service_platform_context(service);
        has_command = context
                      && (context->command_slot.state
                              == RDX_STORAGE_COMMAND_SLOT_PENDING
                          || context->command_slot.state
                              == RDX_STORAGE_COMMAND_SLOT_ABANDONED);
        has_operation = service->operation_pending;
        has_upload_close = service->upload_close_pending;
        has_work = service_write_ready_locked(service);
        service_state_unlock(service);
        if (has_command) {
            service_process_command(service);
            continue;
        }
        if (has_operation) {
            service_process_operation(service);
            continue;
        }
        if (has_upload_close) {
            service_process_upload_close(service);
            continue;
        }
        if (has_work) {
            if (rdx_local_storage_service_pump(
                    service, RDX_LOCAL_STORAGE_PUMP_BUDGET)
                != RDX_STORAGE_RESULT_READY) {
                /* service_fail() has emitted the first error and detached the
                 * capture.  Leave the worker alive for a later START. */
                continue;
            }
            continue;
        }
        os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        service_state_lock(service);
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
        if (context && context->wake_latency_pending) {
            service_diag_max(
                &context->wake_latency_max_ms,
                sys_timer_get_ms() - context->wake_post_ms);
            context->wake_latency_pending = 0;
        }
#endif
        service->worker_wake_pending = 0;
        service_state_unlock(service);
    }
}
#endif

int rdx_local_storage_service_init(
    struct rdx_local_storage_service *service,
    enum rdx_local_storage_backend backend,
    const struct rdx_storage_media_ops *media_ops,
    const struct rdx_native_artifact_ops *native_ops,
    const struct rdx_legacy_uxfile_ops *legacy_ops,
    rdx_local_storage_completion_callback_t completion_callback,
    void *completion_priv)
{
    if (!service || !media_ops || !media_ops->ensure_ready
        || (backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
            && (!native_ops || !native_ops->open_temp || !native_ops->append
                || !native_ops->close || !native_ops->publish
                || !native_ops->store_marker || !native_ops->load_index
                || !native_ops->commit_index || !native_ops->quarantine))
        || (backend == RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE
            && (!legacy_ops || !legacy_ops->init || !legacy_ops->begin_file
                || !legacy_ops->raw_write || !legacy_ops->flush_commit
                || !legacy_ops->abort_file))) {
        return -1;
    }
    if (backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && backend != RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE) {
        return -1;
    }
#if CONFIG_CPU_BR56
    /* Reject a second target instance before touching caller-owned storage;
     * the live worker may still be using the first Service object. */
    if (rdx_local_storage_platform_context.owner) {
        return -1;
    }
#endif
    memset(service, 0, sizeof(*service));
    service->backend = backend;
    service->readiness = RDX_STORAGE_READINESS_UNINITIALIZED;
    service->media_ops = media_ops;
    service->native_ops = native_ops;
    service->legacy_ops = legacy_ops;
    service->completion_callback = completion_callback;
    service->completion_priv = completion_priv;
#if CONFIG_CPU_BR56
    memset(&rdx_local_storage_platform_context, 0,
           sizeof(rdx_local_storage_platform_context));
    rdx_local_storage_platform_context.owner = service;
    os_mutex_create(&rdx_local_storage_platform_context.executor_lock);
    os_sem_create(&rdx_local_storage_platform_context.command_done, 0);
    spin_lock_init(&rdx_local_storage_platform_context.state_lock);
    rdx_storage_command_slot_init(
        &rdx_local_storage_platform_context.command_slot);
    service->platform_context = &rdx_local_storage_platform_context;
    service->platform_initialized = 1;
    service->worker_stop = 0;
    if (task_create(rdx_local_storage_service_task, service,
                    RDX_LOCAL_STORAGE_TASK_NAME)) {
        service->platform_initialized = 0;
        service->platform_context = NULL;
        rdx_local_storage_platform_context.owner = NULL;
        return -1;
    }
    service->worker_created = 1;
#endif
    return 0;
}

static enum rdx_storage_result service_recover_executor(
    struct rdx_local_storage_service *service)
{
    enum rdx_storage_result result;

    if (!service || !service->media_ops || !service->media_ops->ensure_ready) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    service->readiness = RDX_STORAGE_READINESS_RECOVERING;
    result = service->media_ops->ensure_ready();
    if (result == RDX_STORAGE_RESULT_READY
        && service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops && service->native_ops->recover) {
        result = service->native_ops->recover();
    }
    if (result == RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_READY;
        service->recovery_checked = 1;
    } else if (result == RDX_STORAGE_RESULT_NOT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service->recovery_checked = 0;
    } else {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
    }
    return result;
}

enum rdx_storage_result rdx_local_storage_service_recover(
    struct rdx_local_storage_service *service)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_RECOVER;
    return service_run_command(service, &command, 0);
}

static const char *service_filename_basename(const char *name)
{
    const char *slash;

    if (!name) {
        return "";
    }
    slash = strrchr(name, '/');
    return slash ? slash + 1 : name;
}

static uint8_t service_filename_exists(
    const struct rdx_storage_index_snapshot *snapshot,
    const char *filename)
{
    uint32_t i;

    if (!snapshot || !filename || !filename[0]) {
        return 0;
    }
    for (i = 0; i < snapshot->count; i++) {
        const char *existing = service_filename_basename(
            snapshot->entries[i].file.file_id.name);

        if (!strcmp(existing, filename)) {
            return 1;
        }
    }
    return 0;
}

static void service_filename_random_bytes(uint32_t serial_number,
                                          uint32_t attempt,
                                          uint8_t random_bytes[3])
{
#if CONFIG_CPU_BR56
    (void)serial_number;
    (void)attempt;
    get_random_number(random_bytes, 3);
#else
    /* Host tests need repeatable names while the target follows the reference
     * implementation and draws three bytes from JL's random provider. */
    uint32_t value = serial_number ^ ((attempt + 1) * 0x9e3779b9UL);

    value ^= value >> 16;
    value *= 0x7feb352dUL;
    value ^= value >> 15;
    random_bytes[0] = (uint8_t)value;
    random_bytes[1] = (uint8_t)(value >> 8);
    random_bytes[2] = (uint8_t)(value >> 16);
#endif
}

static int service_generate_compat_filename(
    const struct rdx_storage_index_snapshot *snapshot,
    uint32_t serial_number, char *filename, uint32_t capacity)
{
    uint32_t attempt;

    if (!snapshot || !serial_number || !filename || !capacity) {
        return -1;
    }
    for (attempt = 0; attempt < RDX_COMPAT_FILENAME_ATTEMPTS; attempt++) {
        uint8_t random_bytes[3];
        int len;

        service_filename_random_bytes(serial_number, attempt, random_bytes);
        len = snprintf(filename, capacity, "%x%x%x.raw",
                       random_bytes[0], random_bytes[1], random_bytes[2]);
        if (len <= 0 || (uint32_t)len >= capacity) {
            return -1;
        }
        if (!service_filename_exists(snapshot, filename)) {
            return 0;
        }
    }
    filename[0] = 0;
    return -1;
}

static int service_fill_reserved_spec(
    enum rdx_record_format_id format_id, uint32_t serial_number,
    const struct rdx_storage_index_snapshot *snapshot,
    struct rdx_storage_artifact_spec *spec)
{
    char filename[RDX_STORAGE_NAME_MAX];
    int temp_len;
    int final_len;

    if (!spec || !serial_number || !snapshot) {
        return -1;
    }
    memset(spec, 0, sizeof(*spec));
    spec->artifact_id.value = serial_number;
    spec->serial_number = serial_number;
    spec->format_id = format_id;
    switch (format_id) {
    case RDX_RECORD_FORMAT_MEETING_V1:
        spec->sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE;
        spec->channels = RDX_RECORD_MEETING_V1_CHANNELS;
        spec->frame_ms = RDX_RECORD_MEETING_V1_FRAME_MS;
        spec->payload_size = RDX_RECORD_MEETING_V1_PAYLOAD_SIZE;
        break;
    case RDX_RECORD_FORMAT_CALL_V1:
        spec->sample_rate = RDX_RECORD_CALL_V1_SAMPLE_RATE;
        spec->channels = RDX_RECORD_CALL_V1_CHANNELS;
        spec->frame_ms = RDX_RECORD_CALL_V1_FRAME_MS;
        spec->payload_size = RDX_RECORD_CALL_V1_PAYLOAD_SIZE;
        break;
    default:
        return -1;
    }
    if (service_generate_compat_filename(snapshot, serial_number,
                                         filename, sizeof(filename))) {
        memset(spec, 0, sizeof(*spec));
        return -1;
    }
    temp_len = snprintf(spec->temp_name, sizeof(spec->temp_name),
                        "RDXTMP/T%08X.OPU", (unsigned int)serial_number);
    /* Legacy UXFILE and the APP file commands address recordings by a bare
     * filename below storage/sd0/C/.  Keep the published Artifact in that
     * root so list/read/delete remain wire-compatible with the reference
     * implementation; only transactional files live in internal folders. */
    final_len = snprintf(spec->final_name, sizeof(spec->final_name),
                         "%s", filename);
    if (temp_len <= 0 || temp_len >= sizeof(spec->temp_name)
        || final_len <= 0 || final_len >= sizeof(spec->final_name)) {
        memset(spec, 0, sizeof(*spec));
        return -1;
    }
    return 0;
}

static enum rdx_storage_result service_reserve_artifact_executor(
    struct rdx_local_storage_service *service,
    enum rdx_record_format_id format_id,
    struct rdx_storage_artifact_spec *spec)
{
    struct rdx_storage_index_snapshot snapshot;
    enum rdx_storage_result result;
    uint32_t serial_number;

    if (!service || !spec) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service->backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE
        || !service->native_ops || !service->native_ops->load_index
        || !service->native_ops->commit_index) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    if ((service->native_ops->recover && !service->recovery_checked)
        || service->readiness == RDX_STORAGE_READINESS_RECOVERY_REQUIRED
        || service->readiness == RDX_STORAGE_READINESS_HOST_OWNED
        || service->readiness == RDX_STORAGE_READINESS_QUIESCING) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    result = service->media_ops->ensure_ready();
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service->recovery_checked = 0;
        return result;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = service->scratch.index_entries;
    snapshot.capacity = sizeof(service->scratch.index_entries)
                        / sizeof(service->scratch.index_entries[0]);
    result = service->native_ops->load_index(&snapshot);
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
        return result;
    }
    if (snapshot.count >= snapshot.capacity) {
        return RDX_STORAGE_RESULT_NO_SPACE;
    }
    if (snapshot.serial_high_water == UINT32_MAX) {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    serial_number = snapshot.serial_high_water + 1;
    if (service_fill_reserved_spec(format_id, serial_number, &snapshot,
                                   spec)) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    snapshot.serial_high_water = serial_number;
    result = service->native_ops->commit_index(&snapshot);
    if (result != RDX_STORAGE_RESULT_READY) {
        memset(spec, 0, sizeof(*spec));
        if (result == RDX_STORAGE_RESULT_CORRUPT
            || result == RDX_STORAGE_RESULT_CHECKSUM) {
            service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
            service->recovery_checked = 0;
        }
        return result;
    }
    service_state_lock(service);
    service->index_generation = snapshot.generation + 1;
    service_state_unlock(service);
    printf("[RDX][LOCAL] reserved artifact=%08X sn=%u format=%u"
           " temp=%s final=%s\n",
           (unsigned int)spec->artifact_id.value,
           (unsigned int)spec->serial_number, spec->format_id,
           spec->temp_name, spec->final_name);
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_reserve_artifact(
    struct rdx_local_storage_service *service,
    enum rdx_record_format_id format_id,
    struct rdx_storage_artifact_spec *spec)
{
    struct service_command command;
    struct service_command_output output;

    if (!spec) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&command, 0, sizeof(command));
    memset(&output, 0, sizeof(output));
    command.type = SERVICE_COMMAND_RESERVE_ARTIFACT;
    command.data.reserve.format_id = format_id;
    output.reserved_spec = spec;
    return service_run_command_internal(
        service, &command, 0, &output);
}

static enum rdx_storage_result service_start_executor(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_artifact_spec *spec)
{
    enum rdx_storage_result result;

    if (!service || !spec || !spec->artifact_id.value
        || !spec->serial_number || !spec->frame_ms || !spec->payload_size
        || spec->payload_size > RDX_RECORD_CANDIDATE_MAX_SIZE
        || !spec->temp_name[0] || !spec->final_name[0]) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops && service->native_ops->recover
        && !service->recovery_checked) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (service->readiness == RDX_STORAGE_READINESS_RECOVERY_REQUIRED
        || service->readiness == RDX_STORAGE_READINESS_HOST_OWNED
        || service->readiness == RDX_STORAGE_READINESS_QUIESCING) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service->readiness = RDX_STORAGE_READINESS_PROBING;
    result = service->media_ops->ensure_ready();
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
            && service->native_ops && service->native_ops->recover) {
            /* The mount may represent a replaced or power-cycled medium when
             * it becomes available again.  Do not reuse recovery evidence
             * obtained before the readiness failure. */
            service->recovery_checked = 0;
        }
        return result;
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE) {
        result = service->legacy_ops->init();
        if (result != RDX_STORAGE_RESULT_READY) {
            service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
            return result;
        }
    }
    service->artifact_handle = NULL;
    service->session_id = spec->session_id;
    service->capture_generation = 0;
    service->capture_detached = 0;
    service->seal_requested = 0;
    service->completion_sent = 0;
    service->queue_head = 0;
    service->queue_tail = 0;
    service->queue_count = 0;
    service->checkpoint_persisted_frame_count = 0;
    rdx_artifact_init(&service->artifact);
    if (rdx_artifact_begin(&service->artifact, spec)) {
        service->readiness = RDX_STORAGE_READINESS_READY;
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&service->file_id, 0, sizeof(service->file_id));
    service->file_id.artifact_id = spec->artifact_id;
    service->file_id.serial_number = spec->serial_number;
    memcpy(service->file_id.name, spec->final_name,
           sizeof(service->file_id.name));
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        struct rdx_storage_marker marker;

        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_ALLOCATED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
        result = service->native_ops->open_temp(
            spec, &service->artifact_handle);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
    } else {
        result = service->legacy_ops->begin_file(spec, &service->file_id);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
    }
    if (rdx_artifact_mark_writing(&service->artifact)) {
        return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                            RDX_RECORD_CAUSE_START_FAILED);
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        struct rdx_storage_marker marker;

        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_WRITING,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
    }
    service->active = 1;
    service->readiness = RDX_STORAGE_READINESS_READY;
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_start(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_artifact_spec *spec)
{
    struct service_command command;

    if (!spec) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_START;
    command.data.start.spec = *spec;
    return service_run_command(service, &command, 0);
}

static enum rdx_storage_result service_attach_executor(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    if (!service || !service->active || !capture_generation
        || (service->session_id && service->session_id != session_id)
        || service->capture_generation) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    /* A coordinator may have to prepare the artifact before Record Engine
     * allocates its session token.  Bind that wildcard exactly once at the
     * attach cut, then persist the real session id in the WRITING marker. */
    if (!service->session_id) {
        service->session_id = session_id;
        service->artifact.spec.session_id = session_id;
        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
            && service->native_ops && service->native_ops->store_marker) {
            struct rdx_storage_marker marker;

            service_fill_marker(service, RDX_STORAGE_MARKER_STATE_WRITING,
                                &marker);
            if (service->native_ops->store_marker(&marker)
                != RDX_STORAGE_RESULT_READY) {
                return service_fail(service, RDX_STORAGE_RESULT_IO_ERROR,
                                    RDX_RECORD_CAUSE_START_FAILED);
            }
        }
    }
    service->capture_generation = capture_generation;
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_attach(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_ATTACH;
    command.data.attach.session_id = session_id;
    command.data.attach.capture_generation = capture_generation;
    return service_run_command(service, &command, 0);
}

enum rdx_storage_result rdx_local_storage_service_rebind_capture(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t old_capture_generation, uint32_t new_capture_generation)
{
    enum rdx_storage_result result = RDX_STORAGE_RESULT_BAD_STATE;

    if (!service || !session_id || !old_capture_generation
        || !new_capture_generation
        || old_capture_generation == new_capture_generation) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    service_state_lock(service);
    if (service_ids_match(service, session_id, old_capture_generation)
        && !service->seal_requested && !service->capture_detached) {
        /* Queued slots already own copied payload bytes and do not use the
         * capture token while the storage worker drains them.  Updating the
         * admission token therefore preserves pre-pause frames and rejects
         * only stale callbacks from the closed capture instance. */
        service->capture_generation = new_capture_generation;
        result = RDX_STORAGE_RESULT_READY;
    }
    service_state_unlock(service);
    return result;
}

static enum rdx_storage_result service_cancel_start_executor(
    struct rdx_local_storage_service *service, enum rdx_record_cause cause)
{
    if (!service || !service->active || service->capture_generation) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (cause == RDX_RECORD_CAUSE_NONE) {
        cause = RDX_RECORD_CAUSE_START_FAILED;
    }
    return service_fail(service, RDX_STORAGE_RESULT_IO_ERROR, cause);
}

enum rdx_storage_result rdx_local_storage_service_cancel_start(
    struct rdx_local_storage_service *service, enum rdx_record_cause cause)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_CANCEL_START;
    command.data.cancel_start.cause = cause;
    return service_run_command(service, &command, 0);
}

enum rdx_storage_result rdx_local_storage_service_consume(
    struct rdx_local_storage_service *service,
    const struct rdx_record_frame *frame)
{
    struct rdx_local_storage_frame_slot *slot;
    uint8_t wake_worker;

    if (!service || !frame || !service_ids_match(
                                service, frame->session_id,
                                frame->capture_generation)
        || service->seal_requested || service->capture_detached) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service_state_lock(service);
    /* The worker may have failed the artifact after the first check. */
    if (!service_ids_match(service, frame->session_id,
                           frame->capture_generation)
        || service->seal_requested || service->capture_detached) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (!frame->payload || !frame->payload_len
        || frame->payload_len > RDX_RECORD_CANDIDATE_MAX_SIZE
        || frame->payload_len != service->artifact.spec.payload_size) {
        service->artifact.rejected_frame_count++;
        rdx_artifact_fail(&service->artifact,
                          RDX_RECORD_CAUSE_FORMAT_MISMATCH);
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (rdx_artifact_note_produced(&service->artifact)) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (service->queue_count >= RDX_LOCAL_STORAGE_QUEUE_DEPTH) {
        service->artifact.rejected_frame_count++;
        service->artifact.dropped_frame_count++;
        rdx_artifact_fail(&service->artifact, RDX_RECORD_CAUSE_QUEUE_FULL);
        service_state_unlock(service);
        service_diag_failure("local_queue_full");
        return RDX_STORAGE_RESULT_QUEUE_FULL;
    }
    slot = &service->queue[service->queue_tail];
    slot->frame = *frame;
    memcpy(slot->payload, frame->payload, frame->payload_len);
    slot->frame.payload = slot->payload;
    if (rdx_artifact_note_destination_accepted(&service->artifact)
        || rdx_artifact_accept_frame(&service->artifact, frame->payload_len)) {
        service->artifact.rejected_frame_count++;
        rdx_artifact_fail(&service->artifact, RDX_RECORD_CAUSE_STORAGE_ERROR);
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service->queue_tail = (service->queue_tail + 1)
                          % RDX_LOCAL_STORAGE_QUEUE_DEPTH;
    service->queue_count++;
#if CONFIG_CPU_BR56
    {
        struct rdx_local_storage_platform_context *context =
            service_platform_context(service);

        if (context) {
            service_diag_queue_peak_locked(context, service->queue_count);
        }
    }
#endif
    wake_worker = service_write_ready_locked(service);
    service_state_unlock(service);
    if (wake_worker) {
        service_wake(service);
    }
    return RDX_STORAGE_RESULT_READY;
}

/* The caller owns executor_lock.  Move each batch into worker-owned scratch
 * and release its queue slots under the short state lock before entering the
 * potentially blocking backend.  The producer therefore keeps the complete
 * bounded queue as FAT-latency headroom instead of losing the in-flight slots. */
static enum rdx_storage_result service_pump_executor(
    struct rdx_local_storage_service *service, uint8_t budget)
{
    uint8_t pumped = 0;
    uint8_t checkpoint_required = 0;
    uint32_t checkpoint_frame_count = 0;
    uint32_t checkpoint_generation = 0;
    struct rdx_storage_marker checkpoint_marker;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    while (!budget || pumped < budget) {
        uint8_t batch_frames = 1;
        uint8_t persisted_in_batch = 0;
        uint8_t persist_failed = 0;
        uint32_t batch_bytes;
        uint32_t frame_bytes;
        uint32_t artifact_generation;
        uint32_t written = 0;
        enum rdx_storage_result result;

        service_state_lock(service);
        if (!service->active) {
            service_state_unlock(service);
            return RDX_STORAGE_RESULT_BAD_STATE;
        }
        if (!service->queue_count) {
            service_state_unlock(service);
            break;
        }
        artifact_generation = service->artifact.generation;
        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
            uint8_t index = service->queue_head;

            batch_frames = 0;
            batch_bytes = 0;
            while (batch_frames < service->queue_count
                   && batch_frames
                      < RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES
                   && (!budget || pumped + batch_frames < budget)) {
                struct rdx_local_storage_frame_slot *slot =
                    &service->queue[index];

                if (slot->frame.payload_len
                    > sizeof(service->scratch.write_batch) - batch_bytes) {
                    break;
                }
                memcpy(service->scratch.write_batch + batch_bytes,
                       slot->payload,
                       slot->frame.payload_len);
                batch_bytes += slot->frame.payload_len;
                batch_frames++;
                index = (index + 1) % RDX_LOCAL_STORAGE_QUEUE_DEPTH;
            }
            if (!batch_frames) {
                service_state_unlock(service);
                return service_fail(service,
                                    RDX_STORAGE_RESULT_BAD_ARGUMENT,
                                    RDX_RECORD_CAUSE_STORAGE_ERROR);
            }
            service->queue_head = index;
        } else {
            struct rdx_local_storage_frame_slot *slot =
                &service->queue[service->queue_head];

            batch_bytes = slot->frame.payload_len;
            memcpy(service->scratch.write_batch, slot->payload,
                   batch_bytes);
            service->queue_head = (service->queue_head + 1)
                                  % RDX_LOCAL_STORAGE_QUEUE_DEPTH;
        }
        frame_bytes = batch_bytes / batch_frames;
        service->queue_count -= batch_frames;
#if CONFIG_CPU_BR56 && RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
        {
            struct rdx_local_storage_platform_context *context =
                service_platform_context(service);

            if (context) {
                context->frame_batches++;
            }
        }
#endif
        service_state_unlock(service);

        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
            result = service->native_ops->append(
                service->artifact_handle, service->scratch.write_batch,
                batch_bytes, &written);
        } else {
            result = service->legacy_ops->raw_write(
                &service->file_id, service->scratch.write_batch,
                batch_bytes, &written);
        }
        if (result != RDX_STORAGE_RESULT_READY
            || written != batch_bytes) {
            return service_fail(service,
                                result == RDX_STORAGE_RESULT_READY
                                    ? RDX_STORAGE_RESULT_IO_ERROR : result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service_state_lock(service);
        if (!service->active
            || service->artifact.generation != artifact_generation) {
            service_state_unlock(service);
            return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        while (batch_frames) {
            const uint8_t *payload =
                service->scratch.write_batch
                + persisted_in_batch * frame_bytes;

            if (rdx_artifact_persist_frame(
                    &service->artifact, frame_bytes,
                    service_crc32_update(
                        service->artifact.audio_crc,
                        service->artifact.persisted_frame_count,
                        payload, frame_bytes))) {
                persist_failed = 1;
                break;
            }
            persisted_in_batch++;
            pumped++;
            batch_frames--;
        }
        service_state_unlock(service);
        if (persist_failed) {
            return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
    }

    service_state_lock(service);
    if (RDX_CFG_LOCAL_STORAGE_PERIODIC_CHECKPOINT_ENABLE
        && pumped
        && service->active
        && service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops->store_marker
        && ((uint64_t)(service->artifact.persisted_frame_count
                       - service->checkpoint_persisted_frame_count)
            * service->artifact.spec.frame_ms
            >= RDX_LOCAL_STORAGE_CHECKPOINT_INTERVAL_MS)) {
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_WRITING,
                            &checkpoint_marker);
        checkpoint_required = 1;
        checkpoint_frame_count = service->artifact.persisted_frame_count;
        checkpoint_generation = service->artifact.generation;
    }
    service_state_unlock(service);

    if (checkpoint_required) {
        if (service->native_ops->store_marker(&checkpoint_marker)
            != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, RDX_STORAGE_RESULT_IO_ERROR,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service_state_lock(service);
        if (service->active
            && service->artifact.generation == checkpoint_generation) {
            service->checkpoint_persisted_frame_count =
                checkpoint_frame_count;
        }
        service_state_unlock(service);
    }
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_pump(
    struct rdx_local_storage_service *service, uint8_t budget)
{
    enum rdx_storage_result result;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
#if CONFIG_CPU_BR56
    /* Target FAT ownership belongs to rdx_storage.  The public pump remains
     * available to deterministic Host tests but must not create a second
     * target execution path. */
    if (!os_current_task()
        || strcmp(os_current_task(), RDX_LOCAL_STORAGE_TASK_NAME)) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
#endif
    service_executor_lock(service);
    result = service_pump_executor(service, budget);
    service_executor_unlock(service);
    return result;
}

static enum rdx_storage_result service_seal_executor(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    struct rdx_storage_marker marker;
    struct rdx_storage_index_snapshot snapshot;
    enum rdx_storage_result result;
    uint64_t file_size = 0;
    uint32_t audio_crc = 0;

    service_state_lock(service);
    if (!service_ids_match(service, session_id, capture_generation)
        || !service->seal_requested) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service_state_unlock(service);
    /* SEAL is executed by the storage owner after the STOP cut has closed
     * frame admission.  Drain every accepted frame here before the terminal
     * marker, publish and index transaction. */
    result = service_pump_executor(service, 0);
    if (result != RDX_STORAGE_RESULT_READY) {
        return result;
    }
    service_state_lock(service);
    if (!service_ids_match(service, session_id, capture_generation)
        || !service->seal_requested) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service_state_unlock(service);
    if (rdx_artifact_queue_seal(&service->artifact)
        || rdx_artifact_begin_commit(&service->artifact)) {
        return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                            RDX_RECORD_CAUSE_STORAGE_ERROR);
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        result = service->native_ops->close(service->artifact_handle,
                                            &file_size, &audio_crc);
        printf("[RDX][LOCAL] seal stage=close result=%u"
               " bytes_hi=%u bytes_lo=%u crc=%08X\n",
               result, (unsigned int)(file_size >> 32),
               (unsigned int)file_size, (unsigned int)audio_crc);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->artifact_handle = NULL;
        if (file_size != service->artifact.audio_bytes
            || (service->artifact.persisted_frame_count
                && audio_crc != service->artifact.audio_crc)) {
            printf("[RDX][LOCAL] seal stage=verify result=%u"
                   " expected_bytes_hi=%u expected_bytes_lo=%u"
                   " expected_crc=%08X\n",
                   RDX_STORAGE_RESULT_CORRUPT,
                   (unsigned int)(service->artifact.audio_bytes >> 32),
                   (unsigned int)service->artifact.audio_bytes,
                   (unsigned int)service->artifact.audio_crc);
            return service_fail(service, RDX_STORAGE_RESULT_CORRUPT,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->artifact.audio_crc = audio_crc;
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_SEALED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        printf("[RDX][LOCAL] seal stage=sealed_marker result=%u\n",
               result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        result = service->native_ops->publish(
            service->artifact.spec.temp_name, service->artifact.spec.final_name);
        printf("[RDX][LOCAL] seal stage=publish result=%u\n", result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_PUBLISHED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        printf("[RDX][LOCAL] seal stage=published_marker result=%u\n",
               result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.entries = service->scratch.index_entries;
        snapshot.capacity = sizeof(service->scratch.index_entries)
                            / sizeof(service->scratch.index_entries[0]);
        result = service->native_ops->load_index(&snapshot);
        printf("[RDX][LOCAL] seal stage=load_index result=%u"
               " generation=%u count=%u high_water=%u\n",
               result, (unsigned int)snapshot.generation,
               (unsigned int)snapshot.count,
               (unsigned int)snapshot.serial_high_water);
        if (result != RDX_STORAGE_RESULT_READY
            || snapshot.count >= snapshot.capacity) {
            return service_fail(
                service,
                result == RDX_STORAGE_RESULT_READY
                    ? RDX_STORAGE_RESULT_NO_SPACE : result,
                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        snapshot.entries[snapshot.count].file.file_id = service->file_id;
        snapshot.entries[snapshot.count].file.state =
            RDX_ARTIFACT_STATE_COMMITTED;
        snapshot.entries[snapshot.count].file.file_size = file_size;
        snapshot.entries[snapshot.count].file.frame_count =
            service->artifact.persisted_frame_count;
        snapshot.entries[snapshot.count].file.audio_crc = audio_crc;
        snapshot.count++;
        result = service->native_ops->commit_index(&snapshot);
        printf("[RDX][LOCAL] seal stage=commit_index result=%u"
               " generation=%u count=%u high_water=%u\n",
               result, (unsigned int)(snapshot.generation + 1),
               (unsigned int)snapshot.count,
               (unsigned int)snapshot.serial_high_water);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service_state_lock(service);
        service->index_generation = snapshot.generation + 1;
        service_state_unlock(service);
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_INDEXED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        printf("[RDX][LOCAL] seal stage=indexed_marker result=%u\n",
               result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        if (rdx_artifact_commit(&service->artifact)) {
            return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->active = 0;
        service->capture_detached = 1;
        service_emit(service, RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_COMMITTED,
                     RDX_STORAGE_RESULT_READY, RDX_RECORD_CAUSE_NONE);
    } else {
        result = service->legacy_ops->flush_commit(&service->file_id);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        if (rdx_artifact_commit(&service->artifact)) {
            return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->active = 0;
        service->capture_detached = 1;
        service_emit(service,
                     RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_COMMITTED,
                     RDX_STORAGE_RESULT_READY, RDX_RECORD_CAUSE_NONE);
    }
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_seal(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_SEAL;
    command.data.seal.session_id = session_id;
    command.data.seal.capture_generation = capture_generation;
    return service_run_command(service, &command, 1);
}

static enum rdx_storage_result service_abort_executor(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation, enum rdx_record_cause cause)
{
    if (!service_ids_match(service, session_id, capture_generation)) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (cause == RDX_RECORD_CAUSE_NONE) {
        cause = RDX_RECORD_CAUSE_STORAGE_ERROR;
    }
    return service_fail(service, RDX_STORAGE_RESULT_IO_ERROR, cause);
}

enum rdx_storage_result rdx_local_storage_service_abort(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation, enum rdx_record_cause cause)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_ABORT;
    command.data.abort.session_id = session_id;
    command.data.abort.capture_generation = capture_generation;
    command.data.abort.cause = cause;
    return service_run_command(service, &command, 0);
}

void rdx_local_storage_service_detach_capture(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    service_state_lock(service);
    if (service_ids_match(service, session_id, capture_generation)) {
        service->capture_detached = 1;
    }
    service_state_unlock(service);
}

enum rdx_storage_readiness rdx_local_storage_service_readiness(
    const struct rdx_local_storage_service *service)
{
    return service ? service->readiness : RDX_STORAGE_READINESS_UNINITIALIZED;
}

const struct rdx_artifact *rdx_local_storage_service_artifact(
    const struct rdx_local_storage_service *service)
{
    return service ? &service->artifact : NULL;
}

static enum rdx_storage_result service_get_capacity_executor(
    struct rdx_local_storage_service *service, uint64_t *total_bytes,
    uint64_t *free_bytes)
{
    if (!service || !service->media_ops || !service->media_ops->get_capacity) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    return service->media_ops->get_capacity(total_bytes, free_bytes);
}

enum rdx_storage_result rdx_local_storage_service_get_capacity(
    struct rdx_local_storage_service *service, uint64_t *total_bytes,
    uint64_t *free_bytes)
{
    struct service_command command;
    struct service_command_output output;

    if (!total_bytes || !free_bytes) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&command, 0, sizeof(command));
    memset(&output, 0, sizeof(output));
    command.type = SERVICE_COMMAND_GET_CAPACITY;
    output.total_bytes = total_bytes;
    output.free_bytes = free_bytes;
    return service_run_command_internal(
        service, &command, 0, &output);
}

static enum rdx_storage_result service_enumerate_executor(
    struct rdx_local_storage_service *service,
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count)
{
    enum rdx_storage_result result;
    uint32_t generation = 0;

    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->enumerate) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    result = service->media_ops->enumerate(
        items, capacity, count, &generation);
    if (result == RDX_STORAGE_RESULT_READY) {
        service_state_lock(service);
        service->index_generation = generation;
        service_state_unlock(service);
    }
    return result;
}

enum rdx_storage_result rdx_local_storage_service_enumerate(
    struct rdx_local_storage_service *service,
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count)
{
    struct service_command command;
    struct service_command_output output;

    if (!count || (capacity && !items)) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&command, 0, sizeof(command));
    memset(&output, 0, sizeof(output));
    command.type = SERVICE_COMMAND_ENUMERATE;
    command.data.enumerate.capacity = capacity;
    output.items = items;
    output.capacity = capacity;
    output.count = count;
    return service_run_command_internal(
        service, &command, 0, &output);
}

static enum rdx_storage_result service_delete_executor(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_file_id *file_id)
{
    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->delete) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    return service->media_ops->delete(file_id);
}

enum rdx_storage_result rdx_local_storage_service_delete(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_file_id *file_id)
{
    struct service_command command;

    if (!file_id) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_DELETE;
    command.data.delete_file.file_id = *file_id;
    return service_run_command(service, &command, 0);
}

static enum rdx_storage_result service_format_executor(
    struct rdx_local_storage_service *service)
{
    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->format) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    return service_format_and_recover(service, NULL, NULL);
}

enum rdx_storage_result rdx_local_storage_service_format(
    struct rdx_local_storage_service *service)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_FORMAT;
    return service_run_command(service, &command, 0);
}

enum rdx_storage_result rdx_local_storage_service_submit_operation(
    struct rdx_local_storage_service *service,
    const struct rdx_local_storage_operation_request *request,
    rdx_local_storage_operation_callback_t callback, void *priv)
{
    if (!service || !request || !callback || !request->token
        || request->type <= RDX_LOCAL_STORAGE_OPERATION_NONE
        || request->type > RDX_LOCAL_STORAGE_OPERATION_FORMAT) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    switch (request->type) {
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        break;
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        if (!request->data.enumerate.items
            || !request->data.enumerate.capacity) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ:
        if (!request->data.read.serial_number
            || !request->data.read.connection_generation
            || !request->data.read.session_token
            || !request->data.read.buffer || !request->data.read.length) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY:
        if (!request->data.delete_file.serial_number
            || !request->data.delete_file.filename[0]
            || !memchr(request->data.delete_file.filename, '\0',
                       sizeof(request->data.delete_file.filename))) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        break;
    default:
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }

    service_state_lock(service);
    if (service->active || service_operation_busy(service)
        || service_command_busy_locked(service)
        || service->upload_close_pending) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
#if CONFIG_CPU_BR56
    if (!service->worker_created || service->worker_stop) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
#endif
    service->operation = *request;
    service->operation_callback = callback;
    service->operation_priv = priv;
    service->operation_pending = 1;
    service_state_unlock(service);
#if CONFIG_CPU_BR56
    service_wake(service);
#else
    service_process_operation(service);
#endif
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_close_upload(
    struct rdx_local_storage_service *service,
    uint32_t connection_generation, uint32_t session_token)
{
    uint8_t matching_operation = 0;
    uint8_t matching_reader = 0;

    if (!service || !connection_generation || !session_token) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    service_state_lock(service);
    matching_reader = service->upload_reader
        && service->upload_connection_generation == connection_generation
        && service->upload_session_token == session_token;
    matching_operation = service_operation_busy(service)
        && service->operation.type
               == RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ
        && service->operation.data.read.connection_generation
               == connection_generation
        && service->operation.data.read.session_token == session_token;
    if (!matching_reader && !matching_operation) {
        service_state_unlock(service);
        return RDX_STORAGE_RESULT_READY;
    }
    service->upload_close_connection_generation = connection_generation;
    service->upload_close_session_token = session_token;
    service->upload_close_pending = 1;
    service_state_unlock(service);
#if CONFIG_CPU_BR56
    service_wake(service);
#else
    service_process_upload_close(service);
#endif
    return RDX_STORAGE_RESULT_READY;
}

uint32_t rdx_local_storage_service_index_generation(
    struct rdx_local_storage_service *service)
{
    uint32_t generation;

    if (!service) {
        return 0;
    }
    service_state_lock(service);
    generation = service->index_generation;
    service_state_unlock(service);
    return generation;
}

static enum rdx_storage_result service_quiesce_executor(
    struct rdx_local_storage_service *service, uint16_t timeout_ms)
{
    enum rdx_storage_result result;

    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->quiesce) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    service->readiness = RDX_STORAGE_READINESS_QUIESCING;
    result = service->media_ops->quiesce(timeout_ms);
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        return result;
    }
    service->readiness = RDX_STORAGE_READINESS_HOST_OWNED;
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_quiesce(
    struct rdx_local_storage_service *service, uint16_t timeout_ms)
{
    struct service_command command;

    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_QUIESCE;
    command.data.quiesce.timeout_ms = timeout_ms;
    return service_run_command(service, &command, 0);
}

static enum rdx_storage_result service_shutdown_executor(
    struct rdx_local_storage_service *service, uint16_t timeout_ms)
{
    enum rdx_storage_result result = RDX_STORAGE_RESULT_READY;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    if (service->active) {
        result = service_fail(service, RDX_STORAGE_RESULT_IO_ERROR,
                              RDX_RECORD_CAUSE_SHUTDOWN);
    }
    if (service->media_ops && service->media_ops->quiesce) {
        enum rdx_storage_result quiesce_result;

        quiesce_result = service->media_ops->quiesce(timeout_ms);
        if (result == RDX_STORAGE_RESULT_READY) {
            result = quiesce_result;
        }
        if (quiesce_result == RDX_STORAGE_RESULT_READY) {
            service->readiness = RDX_STORAGE_READINESS_HOST_OWNED;
        } else {
            service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        }
    }
    return result;
}

enum rdx_storage_result rdx_local_storage_service_shutdown(
    struct rdx_local_storage_service *service, uint16_t timeout_ms)
{
    struct service_command command;
    enum rdx_storage_result result;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
#if CONFIG_CPU_BR56
    if (!service->platform_initialized) {
        return RDX_STORAGE_RESULT_READY;
    }
#endif
    memset(&command, 0, sizeof(command));
    command.type = SERVICE_COMMAND_SHUTDOWN;
    command.data.shutdown.timeout_ms = timeout_ms;
    result = service_run_command(service, &command, 0);
#if CONFIG_CPU_BR56
    /* A timed-out FAT call is still executing on rdx_storage.  Its command
     * payload and late-completion reconciliation remain owned by that task;
     * killing it here would strand both the filesystem transaction and slot. */
    if (result == RDX_STORAGE_RESULT_TIMEOUT) {
        return result;
    }
    if (!command.token) {
        return result;
    }
    service->worker_stop = 1;
    if (service->worker_created) {
        task_kill(RDX_LOCAL_STORAGE_TASK_NAME);
        service->worker_created = 0;
    }
    if (rdx_local_storage_platform_context.owner == service) {
        service->platform_initialized = 0;
        service->platform_context = NULL;
        rdx_local_storage_platform_context.owner = NULL;
    }
#endif
    return result;
}
