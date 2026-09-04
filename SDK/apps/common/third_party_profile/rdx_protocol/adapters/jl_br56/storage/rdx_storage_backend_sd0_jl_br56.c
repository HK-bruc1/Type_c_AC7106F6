#include "app_config.h"

#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_NATIVE

#include <string.h>

#include "dev_manager.h"
#include "fs/fs.h"
#include "rdx_storage_backend_sd0_jl_br56.h"
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
#include "system/timer.h"
#endif
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE && TCFG_RESOURCE_MONITOR_ENABLE
#include "resource_monitor.h"
#endif

#define RDX_SD0_PATH_MAX       160
#define RDX_SD0_INDEX_MAGIC    0x30584452UL /* RDX0 */
#define RDX_SD0_INDEX_VERSION_V1  1u
#define RDX_SD0_INDEX_VERSION     2u
#define RDX_SD0_INDEX_MAX_ENTRIES  32u
#define RDX_SD0_INDEX_V1_HEADER_SIZE  16u
#define RDX_SD0_INDEX_HEADER_SIZE     20u
#define RDX_SD0_INDEX_ENTRY_SIZE   \
    (8u + 4u + 4u + 8u + 4u + 4u + RDX_STORAGE_NAME_MAX)
#define RDX_SD0_INDEX_TRAILER_SIZE 4u
#define RDX_SD0_INDEX_DATA_SIZE    \
    (RDX_SD0_INDEX_HEADER_SIZE                                     \
     + RDX_SD0_INDEX_MAX_ENTRIES * RDX_SD0_INDEX_ENTRY_SIZE        \
     + RDX_SD0_INDEX_TRAILER_SIZE)
#define RDX_SD0_CRC_READ_CHUNK     (RDX_SD0_INDEX_DATA_SIZE & ~511u)
#define RDX_SD0_MARKER_DATA_SIZE   \
    (2u * 8u + 14u * 4u + 2u * RDX_STORAGE_NAME_MAX + 4u)
#define RDX_SD0_LAYOUT_DIR_COUNT   2u

#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE
#define RDX_SD0_DIAG_BUCKET_COUNT  9u

enum rdx_sd0_diag_operation {
    RDX_SD0_DIAG_OPEN = 0,
    RDX_SD0_DIAG_APPEND,
    RDX_SD0_DIAG_MARKER_WRITE,
    RDX_SD0_DIAG_MARKER_VERIFY,
    RDX_SD0_DIAG_CLOSE_VERIFY,
    RDX_SD0_DIAG_PUBLISH,
    RDX_SD0_DIAG_INDEX_COMMIT,
    RDX_SD0_DIAG_UPLOAD_OPEN,
    RDX_SD0_DIAG_UPLOAD_READ,
    RDX_SD0_DIAG_UPLOAD_CLOSE,
    RDX_SD0_DIAG_OPERATION_COUNT,
};

struct rdx_sd0_diag_metric {
    uint32_t count;
    uint32_t total_ms;
    uint32_t max_ms;
    uint32_t buckets[RDX_SD0_DIAG_BUCKET_COUNT];
};

static const uint32_t rdx_sd0_diag_bucket_upper_ms[
    RDX_SD0_DIAG_BUCKET_COUNT] = {
    0, 1, 2, 5, 10, 20, 50, 100, UINT32_MAX
};
static const char *const rdx_sd0_diag_operation_name[
    RDX_SD0_DIAG_OPERATION_COUNT] = {
    "open", "append", "marker_write", "marker_verify", "close_verify",
    "publish", "index_commit", "upload_open", "upload_read",
    "upload_close"
};
static struct rdx_sd0_diag_metric rdx_sd0_diag_metrics[
    RDX_SD0_DIAG_OPERATION_COUNT];

#define RDX_SD0_DIAG_BEGIN(name)  uint32_t name = sys_timer_get_ms()
#define RDX_SD0_DIAG_END(operation, name)                              \
    rdx_sd0_diag_record((operation), sys_timer_get_ms() - (name))

static void rdx_sd0_diag_record(enum rdx_sd0_diag_operation operation,
                                uint32_t elapsed_ms)
{
    struct rdx_sd0_diag_metric *metric;
    uint32_t i;

    if ((uint32_t)operation >= RDX_SD0_DIAG_OPERATION_COUNT) {
        return;
    }
    metric = &rdx_sd0_diag_metrics[operation];
    metric->count++;
    if (UINT32_MAX - metric->total_ms >= elapsed_ms) {
        metric->total_ms += elapsed_ms;
    } else {
        metric->total_ms = UINT32_MAX;
    }
    if (elapsed_ms > metric->max_ms) {
        metric->max_ms = elapsed_ms;
    }
    for (i = 0; i < RDX_SD0_DIAG_BUCKET_COUNT; i++) {
        if (elapsed_ms <= rdx_sd0_diag_bucket_upper_ms[i]) {
            metric->buckets[i]++;
            break;
        }
    }
}

static uint32_t rdx_sd0_diag_percentile_bound(
    const struct rdx_sd0_diag_metric *metric, uint32_t percent)
{
    uint32_t target;
    uint32_t observed = 0;
    uint32_t i;

    if (!metric || !metric->count) {
        return 0;
    }
    target = (metric->count * percent + 99) / 100;
    for (i = 0; i < RDX_SD0_DIAG_BUCKET_COUNT; i++) {
        observed += metric->buckets[i];
        if (observed >= target) {
            return rdx_sd0_diag_bucket_upper_ms[i];
        }
    }
    return UINT32_MAX;
}

static void rdx_sd0_diag_reset(void)
{
    memset(rdx_sd0_diag_metrics, 0, sizeof(rdx_sd0_diag_metrics));
}

static void rdx_sd0_diag_dump(const char *reason)
{
    uint32_t i;

    for (i = 0; i < RDX_SD0_DIAG_OPERATION_COUNT; i++) {
        const struct rdx_sd0_diag_metric *metric = &rdx_sd0_diag_metrics[i];

        uint32_t average_ms_x100 = metric->count
            ? (uint32_t)(((uint64_t)metric->total_ms * 100)
                         / metric->count)
            : 0;

        printf("[RDX][O0][FAT] reason=%s op=%s count=%u max_ms=%u"
               " avg_ms_x100=%u"
               " p50_le_ms=%u p95_le_ms=%u p99_le_ms=%u\n",
               reason ? reason : "unknown", rdx_sd0_diag_operation_name[i],
               (unsigned int)metric->count, (unsigned int)metric->max_ms,
               (unsigned int)average_ms_x100,
               (unsigned int)rdx_sd0_diag_percentile_bound(metric, 50),
               (unsigned int)rdx_sd0_diag_percentile_bound(metric, 95),
               (unsigned int)rdx_sd0_diag_percentile_bound(metric, 99));
    }
}

static void rdx_sd0_diag_resource(const char *reason, uint8_t include_tasks)
{
#if TCFG_RESOURCE_MONITOR_ENABLE
    resource_monitor_snapshot(reason);
    if (include_tasks) {
        resource_monitor_dump_key_tasks(reason);
    }
#else
    (void)reason;
    (void)include_tasks;
#endif
}

static void rdx_sd0_diag_observe(void)
{
#if TCFG_RESOURCE_MONITOR_ENABLE
    resource_monitor_observe();
#endif
}
#else
#define RDX_SD0_DIAG_BEGIN(name)
#define RDX_SD0_DIAG_END(operation, name)
#define rdx_sd0_diag_reset()
#define rdx_sd0_diag_dump(reason)
#define rdx_sd0_diag_resource(reason, include_tasks)
#define rdx_sd0_diag_observe()
#endif

struct rdx_sd0_open_file {
    FILE *file;
    struct rdx_artifact_id artifact_id;
    char temp_path[RDX_SD0_PATH_MAX];
    char final_path[RDX_SD0_PATH_MAX];
    uint64_t audio_bytes;
    uint8_t pending_rename;
};

struct rdx_sd0_reader {
    FILE *file;
    struct rdx_storage_file_id file_id;
    uint64_t file_size;
    uint64_t next_offset;
};

struct rdx_sd0_backend {
    char root[RDX_SD0_PATH_MAX];
    uint8_t initialized;
    struct rdx_sd0_open_file open;
    struct rdx_sd0_reader reader;
    /* The backend is single-owner. Keep the maximum index image and marker
     * write/readback objects off the storage worker stack. Marker scratch is
     * smaller than the index image, so this union adds no static RAM. */
    union {
        uint8_t index_data[RDX_SD0_INDEX_DATA_SIZE];
        struct {
            uint8_t data[RDX_SD0_MARKER_DATA_SIZE];
            struct rdx_storage_marker decoded;
        } marker;
    } scratch;
    struct rdx_storage_index_entry enumerate_entries[RDX_SD0_INDEX_MAX_ENTRIES];
};

static struct rdx_sd0_backend rdx_sd0;

static int sd0_path(const char *name, char *path, uint32_t capacity);

static uint32_t sd0_crc32_update(uint32_t previous,
                                 uint8_t initialized,
                                 const uint8_t *data, uint32_t len)
{
    uint32_t crc = initialized ? (previous ^ 0xffffffffUL) : 0xffffffffUL;
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

static enum rdx_storage_result sd0_crc_file(const char *path,
                                             uint64_t *file_size,
                                             uint32_t *audio_crc)
{
    /* close/stat and index load are serialized by the Backend owner. Reuse
     * the index image after audio close so verification does not grow BSS or
     * the storage worker stack, and read whole sector batches. */
    uint8_t *buffer = rdx_sd0.scratch.index_data;
    FILE *file;
    uint32_t crc = 0;
    uint8_t initialized = 0;
    uint64_t size = 0;

    if (!path || !file_size || !audio_crc) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    file = fopen(path, "r");
    if (!file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    for (;;) {
        int count = fread(buffer, 1, RDX_SD0_CRC_READ_CHUNK, file);

        if (count < 0) {
            fclose(file);
            return RDX_STORAGE_RESULT_IO_ERROR;
        }
        if (!count) {
            break;
        }
        if ((uint64_t)count > UINT64_MAX - size) {
            fclose(file);
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        crc = sd0_crc32_update(crc, initialized, buffer, (uint32_t)count);
        initialized = 1;
        size += (uint32_t)count;
    }
    fclose(file);
    *file_size = size;
    *audio_crc = initialized ? crc : 0;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_file_size(
    const struct rdx_storage_file_id *file_id, uint64_t *file_size)
{
    char path[RDX_SD0_PATH_MAX];
    FILE *file;

    if (!file_id || !file_size
        || sd0_path(file_id->name, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    file = fopen(path, "r");
    if (!file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    *file_size = flen(file);
    fclose(file);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_ensure_layout(void)
{
    static const char *const names[RDX_SD0_LAYOUT_DIR_COUNT] = {
        "RDXTMP", "RDXBAD",
    };
    uint32_t i;

    for (i = 0; i < RDX_SD0_LAYOUT_DIR_COUNT; i++) {
        char folder[16];
        char path[RDX_SD0_PATH_MAX];

        strncpy(folder, names[i], sizeof(folder) - 1);
        folder[sizeof(folder) - 1] = 0;
        if (snprintf(path, sizeof(path), "%s%s/", rdx_sd0.root,
                     names[i]) >= sizeof(path)) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        if (!fdir_exist(path)) {
            continue;
        }
        if (fmk_dir(rdx_sd0.root, folder, 0) || fdir_exist(path)) {
            return RDX_STORAGE_RESULT_IO_ERROR;
        }
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_load_index(
    struct rdx_storage_index_snapshot *snapshot);
static enum rdx_storage_result sd0_commit_index(
    const struct rdx_storage_index_snapshot *snapshot);
static enum rdx_storage_result sd0_load_marker(
    const struct rdx_artifact_id *artifact_id,
    struct rdx_storage_marker *marker);
static int sd0_marker_path(const struct rdx_artifact_id *artifact_id,
                           char *path, uint32_t capacity);

static enum rdx_storage_result sd0_probe_media(void)
{
    char *root;

    if (!rdx_sd0.initialized
        || !dev_manager_online_check_by_logo("sd0", 1)) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    root = dev_manager_get_root_path_by_logo("sd0");
    if (!root || !root[0]) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    strncpy(rdx_sd0.root, root, sizeof(rdx_sd0.root) - 1);
    rdx_sd0.root[sizeof(rdx_sd0.root) - 1] = 0;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_ready(void)
{
    enum rdx_storage_result result = sd0_probe_media();

    if (result != RDX_STORAGE_RESULT_READY) {
        return result;
    }
    return sd0_ensure_layout();
}

static int sd0_path(const char *name, char *path, uint32_t capacity)
{
    int len;

    if (!name || !name[0] || name[0] == '/' || strstr(name, "..")
        || !path || !capacity) {
        return -1;
    }
    len = snprintf(path, capacity, "%s%s", rdx_sd0.root, name);
    return len > 0 && (uint32_t)len < capacity ? 0 : -1;
}

static enum rdx_storage_result sd0_ensure_ready(void)
{
    return sd0_ready();
}

static enum rdx_storage_result sd0_quiesce(uint16_t timeout_ms)
{
    (void)timeout_ms;
    return sd0_ready();
}

static enum rdx_storage_result sd0_capacity(uint64_t *total_bytes,
                                             uint64_t *free_bytes)
{
    struct vfs_partition *part;
    u32 free_kb = 0;

    if (!total_bytes || !free_bytes) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    part = fget_partition(rdx_sd0.root);
    if (!part || fget_free_space(rdx_sd0.root, &free_kb)) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    /* JL reports both fget_free_space() and vfs_partition::total_size in
     * KiB.  Keep the Media Port byte-based; the compatibility Wire converts
     * both values back to KiB so it matches the reference APP contract. */
    *total_bytes = (uint64_t)part->total_size * 1024ULL;
    *free_bytes = (uint64_t)free_kb * 1024ULL;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_open_reader(
    const struct rdx_storage_file_info *file_info, void **reader)
{
    FILE *file;
    char path[RDX_SD0_PATH_MAX];
    uint64_t file_size;

    if (!file_info || !file_info->file_id.artifact_id.value
        || !file_info->file_id.serial_number || !reader) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (rdx_sd0.reader.file
        || sd0_path(file_info->file_id.name, path, sizeof(path))) {
        return rdx_sd0.reader.file ? RDX_STORAGE_RESULT_BUSY
                                   : RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    rdx_sd0_diag_reset();
    RDX_SD0_DIAG_BEGIN(start_ms);
    file = fopen(path, "r");
    if (!file) {
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_OPEN, start_ms);
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    file_size = flen(file);
    if (file_size != file_info->file_size) {
        fclose(file);
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_OPEN, start_ms);
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    memset(&rdx_sd0.reader, 0, sizeof(rdx_sd0.reader));
    rdx_sd0.reader.file = file;
    rdx_sd0.reader.file_id = file_info->file_id;
    rdx_sd0.reader.file_size = file_size;
    *reader = &rdx_sd0.reader;
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_OPEN, start_ms);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_read_reader(
    void *reader, uint64_t offset, uint8_t *buf,
    uint32_t len, uint32_t *read_len)
{
    struct rdx_sd0_reader *open = reader;
    int count;

    if (open != &rdx_sd0.reader || !open->file || !buf || !read_len
        || !len || offset > 0xffffffffULL
        || offset >= open->file_size) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    RDX_SD0_DIAG_BEGIN(start_ms);
    if (offset != open->next_offset
        && fseek(open->file, (u32)offset, SEEK_SET)) {
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_READ, start_ms);
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    count = fread(buf, 1, len, open->file);
    if (count < 0) {
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_READ, start_ms);
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    *read_len = (uint32_t)count;
    open->next_offset = offset + (uint32_t)count;
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_READ, start_ms);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_close_reader(void *reader)
{
    struct rdx_sd0_reader *open = reader;

    if (open != &rdx_sd0.reader) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    RDX_SD0_DIAG_BEGIN(start_ms);
    if (open->file) {
        fclose(open->file);
    }
    memset(open, 0, sizeof(*open));
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_UPLOAD_CLOSE, start_ms);
    rdx_sd0_diag_dump("upload_close");
    rdx_sd0_diag_resource("upload_close", 1);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_stat(
    const struct rdx_storage_file_id *file_id, uint64_t *file_size,
    uint32_t *audio_crc)
{
    FILE *file;
    char path[RDX_SD0_PATH_MAX];

    if (!file_id || !file_size || !audio_crc) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_path(file_id->name, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    file = fopen(path, "r");
    if (!file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    fclose(file);
    return sd0_crc_file(path, file_size, audio_crc);
}

static enum rdx_storage_result sd0_enumerate(
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count,
    uint32_t *generation)
{
    struct rdx_storage_index_snapshot snapshot;

    if (!count) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (capacity && !items) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(rdx_sd0.enumerate_entries, 0, sizeof(rdx_sd0.enumerate_entries));
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = rdx_sd0.enumerate_entries;
    snapshot.capacity = RDX_SD0_INDEX_MAX_ENTRIES;
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        *count = 0;
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_load_index(&snapshot) != RDX_STORAGE_RESULT_READY) {
        *count = 0;
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    *count = snapshot.count;
    if (generation) {
        *generation = snapshot.generation;
    }
    if (capacity < snapshot.count) {
        return RDX_STORAGE_RESULT_NO_SPACE;
    }
    if (snapshot.count) {
        memcpy(items, rdx_sd0.enumerate_entries,
               snapshot.count * sizeof(struct rdx_storage_index_entry));
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_delete(
    const struct rdx_storage_file_id *file_id)
{
    char path[RDX_SD0_PATH_MAX];
    char marker_path[RDX_SD0_PATH_MAX];
    struct rdx_storage_index_snapshot snapshot;
    struct rdx_storage_index_entry removed;
    uint32_t removed_at;
    uint32_t i;

    if (!file_id || !file_id->artifact_id.value || !file_id->name[0]) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_path(file_id->name, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(rdx_sd0.enumerate_entries, 0,
           sizeof(rdx_sd0.enumerate_entries));
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = rdx_sd0.enumerate_entries;
    snapshot.capacity = RDX_SD0_INDEX_MAX_ENTRIES;
    if (sd0_load_index(&snapshot) != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    removed_at = snapshot.count;
    for (i = 0; i < snapshot.count; i++) {
        if (snapshot.entries[i].file.file_id.artifact_id.value
            == file_id->artifact_id.value) {
            removed_at = i;
            break;
        }
    }
    if (removed_at == snapshot.count
        || strncmp(snapshot.entries[removed_at].file.file_id.name,
                   file_id->name, RDX_STORAGE_NAME_MAX)) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    removed = snapshot.entries[removed_at];
    if (removed_at + 1 < snapshot.count) {
        memmove(&snapshot.entries[removed_at],
                &snapshot.entries[removed_at + 1],
                (snapshot.count - removed_at - 1)
                * sizeof(snapshot.entries[0]));
    }
    snapshot.count--;
    if (sd0_commit_index(&snapshot) != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    if (fdelete_by_name(path)) {
        /* Restore list visibility if the FAT delete did not happen.  A failed
         * rollback is reported as CORRUPT so Service can gate future START. */
        memset(rdx_sd0.enumerate_entries, 0,
               sizeof(rdx_sd0.enumerate_entries));
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.entries = rdx_sd0.enumerate_entries;
        snapshot.capacity = RDX_SD0_INDEX_MAX_ENTRIES;
        if (sd0_load_index(&snapshot) != RDX_STORAGE_RESULT_READY
            || snapshot.count >= snapshot.capacity) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        if (removed_at > snapshot.count) {
            removed_at = snapshot.count;
        }
        if (removed_at < snapshot.count) {
            memmove(&snapshot.entries[removed_at + 1],
                    &snapshot.entries[removed_at],
                    (snapshot.count - removed_at)
                    * sizeof(snapshot.entries[0]));
        }
        snapshot.entries[removed_at] = removed;
        snapshot.count++;
        return sd0_commit_index(&snapshot) == RDX_STORAGE_RESULT_READY
                   ? RDX_STORAGE_RESULT_IO_ERROR
                   : RDX_STORAGE_RESULT_CORRUPT;
    }
    if (!sd0_marker_path(&file_id->artifact_id, marker_path,
                         sizeof(marker_path))) {
        /* A stale terminal marker is not a normal-list entry.  Its cleanup is
         * best effort so an already completed file deletion stays idempotent
         * from the APP's perspective. */
        (void)fdelete_by_name(marker_path);
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_format(void)
{
    int ret;

    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    /* Formatting can exceed the watchdog window.  This follows JL's native
     * UXFILE implementation; the Service immediately reprobes the media and
     * rebuilds the internal layout plus empty A/B index before reporting
     * completion to the APP. */
    wdt_disable();
    ret = f_format(rdx_sd0.root, "fat", 0);
    wdt_enable();
    return ret ? RDX_STORAGE_RESULT_IO_ERROR : RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_open_temp(
    const struct rdx_storage_artifact_spec *spec, void **handle)
{
    struct rdx_sd0_open_file *open;

    if (!spec || !handle) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (rdx_sd0.open.file || rdx_sd0.open.pending_rename) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    if (sd0_path(spec->temp_name, rdx_sd0.open.temp_path,
                 sizeof(rdx_sd0.open.temp_path))
        || sd0_path(spec->final_name, rdx_sd0.open.final_path,
                    sizeof(rdx_sd0.open.final_path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    open = &rdx_sd0.open;
    rdx_sd0_diag_resource("local_open_before", 0);
    RDX_SD0_DIAG_BEGIN(start_ms);
    open->file = fopen(open->temp_path, "w+");
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_OPEN, start_ms);
    if (!open->file) {
        rdx_sd0_diag_resource("local_open_failed", 1);
        rdx_sd0_diag_dump("open_failed");
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    open->artifact_id = spec->artifact_id;
    open->audio_bytes = 0;
    open->pending_rename = 1;
    *handle = open;
    rdx_sd0_diag_resource("local_open_after", 0);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_append(void *handle, const uint8_t *data,
                                          uint32_t len, uint32_t *written)
{
    struct rdx_sd0_open_file *open = handle;
    enum rdx_storage_result result;
    int count;

    if (!open || !open->file || !data || !written) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    RDX_SD0_DIAG_BEGIN(start_ms);
    count = fwrite((void *)data, 1, len, open->file);
    if (count < 0) {
        *written = 0;
        result = RDX_STORAGE_RESULT_IO_ERROR;
        goto done;
    }
    *written = (uint32_t)count;
    if (*written) {
        if (*written > UINT64_MAX - open->audio_bytes) {
            *written = 0;
            result = RDX_STORAGE_RESULT_CORRUPT;
            goto done;
        }
        open->audio_bytes += *written;
    }
    result = *written == len ? RDX_STORAGE_RESULT_READY
                             : RDX_STORAGE_RESULT_IO_ERROR;
done:
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_APPEND, start_ms);
    rdx_sd0_diag_observe();
    if (result != RDX_STORAGE_RESULT_READY) {
        rdx_sd0_diag_resource("local_append_failed", 1);
        rdx_sd0_diag_dump("append_failed");
    }
    return result;
}

static enum rdx_storage_result sd0_close(void *handle, uint64_t *file_size,
                                         uint32_t *audio_crc)
{
    struct rdx_sd0_open_file *open = handle;
    char path[RDX_SD0_PATH_MAX];
    enum rdx_storage_result result;

    if (!open || !open->file || !file_size || !audio_crc) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    strncpy(path, open->temp_path, sizeof(path) - 1);
    path[sizeof(path) - 1] = 0;
    rdx_sd0_diag_resource("local_stop_before_close", 1);
    RDX_SD0_DIAG_BEGIN(start_ms);
    fclose(open->file);
    open->file = NULL;
    /* Re-open the file and calculate the checksum from persisted bytes.  The
     * Service owns the rolling CRC calculated from accepted frame boundaries;
     * this read-back is the independent commit evidence compared by Service. */
    result = sd0_crc_file(path, file_size, audio_crc);
    if (result != RDX_STORAGE_RESULT_READY
        || *file_size != open->audio_bytes) {
        result = result == RDX_STORAGE_RESULT_READY
                     ? RDX_STORAGE_RESULT_CORRUPT : result;
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_CLOSE_VERIFY, start_ms);
        rdx_sd0_diag_resource("local_close_failed", 1);
        rdx_sd0_diag_dump("close_failed");
        return result;
    }
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_CLOSE_VERIFY, start_ms);
    rdx_sd0_diag_resource("local_close_after", 0);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_truncate(void *handle, uint64_t size)
{
    struct rdx_sd0_open_file *open = handle;

    if (!open || !open->file || size > 0xffffffffULL) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    return ftruncate(open->file, (u32)size) ? RDX_STORAGE_RESULT_IO_ERROR
                                            : RDX_STORAGE_RESULT_READY;
}

static int sd0_move_file(FILE *source_file, const char *destination_name)
{
    char destination_path[RDX_SD0_PATH_MAX];
    FILE *moved_file = NULL;
    int path_len;
    int ret;

    if (!source_file || !destination_name || !destination_name[0]
        || destination_name[0] == '/' || strstr(destination_name, "..")) {
        if (source_file) {
            fclose(source_file);
        }
        return -1;
    }
    path_len = snprintf(destination_path, sizeof(destination_path), "/%s",
                        destination_name);
    if (path_len <= 1 || (uint32_t)path_len >= sizeof(destination_path)) {
        fclose(source_file);
        return -1;
    }

    /* Unlike fopen(), the JL VFS fmove() wrapper does not remove the mount
     * prefix before forwarding path_dst to FAT.  FAT therefore needs a
     * volume-relative path (for example, /name.raw), not a VFS path such as
     * storage/sd0/C/name.raw.  path_len is the number of
     * valid path bytes that FAT scans while splitting directory/name. */
    ret = fmove(source_file, destination_path, &moved_file, 0, path_len);
    if (ret) {
        fclose(source_file);
    } else if (moved_file) {
        fclose(moved_file);
    }
    return ret;
}

static enum rdx_storage_result sd0_publish(const char *temp_name,
                                           const char *final_name)
{
    struct rdx_sd0_open_file *open = &rdx_sd0.open;
    char temp_path[RDX_SD0_PATH_MAX];
    char final_path[RDX_SD0_PATH_MAX];
    FILE *source_file;
    int ret;

    if (!open->pending_rename || !temp_name || !final_name
        || sd0_path(temp_name, temp_path, sizeof(temp_path))
        || sd0_path(final_name, final_path, sizeof(final_path))) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (strcmp(temp_path, open->temp_path)
        || strcmp(final_path, open->final_path)) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    RDX_SD0_DIAG_BEGIN(start_ms);
    open->file = fopen(open->temp_path, "r");
    if (!open->file) {
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_PUBLISH, start_ms);
        rdx_sd0_diag_resource("local_publish_failed", 1);
        rdx_sd0_diag_dump("publish_failed");
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    source_file = open->file;
    /* JL frename() only renames within the current directory.  Native
     * publish crosses RDXTMP -> volume root, so use the SDK's fmove() primitive;
     * it closes source_file on success and optionally returns the moved
     * handle for the caller to close. */
    ret = sd0_move_file(source_file, final_name);
    open->file = NULL;
    printf("[RDX][LOCAL] publish temp=%s final=%s fat_path=/%s"
           " path_len=%u ret=%d\n",
           temp_name, final_name, final_name,
           (unsigned int)(strlen(final_name) + 1), ret);
    if (!ret) {
        open->pending_rename = 0;
    }
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_PUBLISH, start_ms);
    if (ret) {
        rdx_sd0_diag_resource("local_publish_failed", 1);
        rdx_sd0_diag_dump("publish_failed");
    }
    return ret ? RDX_STORAGE_RESULT_IO_ERROR : RDX_STORAGE_RESULT_READY;
}

static uint32_t sd0_marker_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xffffffffUL;
    uint32_t i;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        crc = (crc >> 1) ^ (0xedb88320UL & (uint32_t)-(int)(crc & 1));
    }
    return crc;
}

static void sd0_put_u32(uint8_t *buf, uint32_t *offset, uint32_t value)
{
    buf[(*offset)++] = (uint8_t)value;
    buf[(*offset)++] = (uint8_t)(value >> 8);
    buf[(*offset)++] = (uint8_t)(value >> 16);
    buf[(*offset)++] = (uint8_t)(value >> 24);
}

static void sd0_put_u64(uint8_t *buf, uint32_t *offset, uint64_t value)
{
    sd0_put_u32(buf, offset, (uint32_t)value);
    sd0_put_u32(buf, offset, (uint32_t)(value >> 32));
}

static uint32_t sd0_get_u32(const uint8_t *buf, uint32_t *offset)
{
    uint32_t value = (uint32_t)buf[(*offset)++];

    value |= (uint32_t)buf[(*offset)++] << 8;
    value |= (uint32_t)buf[(*offset)++] << 16;
    value |= (uint32_t)buf[(*offset)++] << 24;
    return value;
}

static uint64_t sd0_get_u64(const uint8_t *buf, uint32_t *offset)
{
    uint64_t low = sd0_get_u32(buf, offset);
    uint64_t high = sd0_get_u32(buf, offset);

    return low | (high << 32);
}

static uint32_t sd0_serialize_marker(const struct rdx_storage_marker *marker,
                                     uint8_t *buf, uint32_t capacity)
{
    uint32_t offset = 0;

    if (!marker || !buf || capacity < RDX_SD0_MARKER_DATA_SIZE) {
        return 0;
    }
    sd0_put_u64(buf, &offset, marker->artifact_id.value);
    sd0_put_u32(buf, &offset, marker->generation);
    sd0_put_u32(buf, &offset, marker->state);
    sd0_put_u32(buf, &offset, marker->session_id);
    sd0_put_u32(buf, &offset, marker->serial_number);
    sd0_put_u32(buf, &offset, marker->produced_frame_count);
    sd0_put_u32(buf, &offset, marker->destination_accepted_count);
    sd0_put_u32(buf, &offset, marker->local_accepted_count);
    sd0_put_u32(buf, &offset, marker->persisted_frame_count);
    sd0_put_u32(buf, &offset, marker->rejected_frame_count);
    sd0_put_u32(buf, &offset, marker->dropped_frame_count);
    sd0_put_u32(buf, &offset, marker->first_error_cause);
    sd0_put_u64(buf, &offset, marker->audio_bytes);
    sd0_put_u32(buf, &offset, marker->audio_crc);
    sd0_put_u32(buf, &offset, marker->marks_count);
    sd0_put_u32(buf, &offset, marker->marks_crc);
    memcpy(buf + offset, marker->temp_name, sizeof(marker->temp_name));
    offset += sizeof(marker->temp_name);
    memcpy(buf + offset, marker->final_name, sizeof(marker->final_name));
    offset += sizeof(marker->final_name);
    sd0_put_u32(buf, &offset, sd0_marker_checksum(buf, offset));
    return offset;
}

static int sd0_marker_path(const struct rdx_artifact_id *artifact_id,
                           char *path, uint32_t capacity)
{
    uint32_t high;
    uint32_t low;
    int len;

    if (!artifact_id || !artifact_id->value || !path || !capacity) {
        return -1;
    }
    high = (uint32_t)(artifact_id->value >> 32);
    low = (uint32_t)artifact_id->value;
    len = snprintf(path, capacity, "%sRDXTMP/M%08x%08x.MRK", rdx_sd0.root,
                   (unsigned int)high, (unsigned int)low);
    return len > 0 && (uint32_t)len < capacity ? 0 : -1;
}

static uint8_t sd0_marker_equal(const struct rdx_storage_marker *left,
                                const struct rdx_storage_marker *right)
{
    return left && right
           && left->artifact_id.value == right->artifact_id.value
           && left->generation == right->generation
           && left->state == right->state
           && left->session_id == right->session_id
           && left->serial_number == right->serial_number
           && left->produced_frame_count == right->produced_frame_count
           && left->destination_accepted_count
              == right->destination_accepted_count
           && left->local_accepted_count == right->local_accepted_count
           && left->persisted_frame_count == right->persisted_frame_count
           && left->rejected_frame_count == right->rejected_frame_count
           && left->dropped_frame_count == right->dropped_frame_count
           && left->first_error_cause == right->first_error_cause
           && left->audio_bytes == right->audio_bytes
           && left->audio_crc == right->audio_crc
           && left->marks_count == right->marks_count
           && left->marks_crc == right->marks_crc
           && !memcmp(left->temp_name, right->temp_name,
                      sizeof(left->temp_name))
           && !memcmp(left->final_name, right->final_name,
                      sizeof(left->final_name));
}

static enum rdx_storage_result sd0_store_marker(
    const struct rdx_storage_marker *marker)
{
    uint8_t *data = rdx_sd0.scratch.marker.data;
    struct rdx_storage_marker *verify =
        &rdx_sd0.scratch.marker.decoded;
    char path[RDX_SD0_PATH_MAX];
    FILE *file;
    uint32_t len;
    uint8_t readback_required;

    if (!marker) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_marker_path(&marker->artifact_id, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (marker->state == RDX_STORAGE_MARKER_STATE_ALLOCATED) {
        rdx_sd0_diag_reset();
        rdx_sd0_diag_resource("local_start_before_marker", 1);
    } else if (marker->state == RDX_STORAGE_MARKER_STATE_WRITING) {
        rdx_sd0_diag_resource("writing_marker_before", 0);
    }
    len = sd0_serialize_marker(marker, data, RDX_SD0_MARKER_DATA_SIZE);
    if (len != RDX_SD0_MARKER_DATA_SIZE) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    RDX_SD0_DIAG_BEGIN(write_start_ms);
    file = fopen(path, "w+");
    if (!file || fwrite(data, 1, len, file) != (int)len) {
        if (file) {
            fclose(file);
        }
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_MARKER_WRITE, write_start_ms);
        rdx_sd0_diag_resource("marker_write_failed", 1);
        rdx_sd0_diag_dump("marker_write_failed");
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    fclose(file);
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_MARKER_WRITE, write_start_ms);
    rdx_sd0_diag_resource("marker_write_closed", 0);
    readback_required = RDX_CFG_LOCAL_STORAGE_NONTERMINAL_READBACK_ENABLE
                        || marker->state > RDX_STORAGE_MARKER_STATE_WRITING;
    if (!readback_required) {
        return RDX_STORAGE_RESULT_READY;
    }
    /* A successful fwrite/fclose only proves that the VFS accepted the
     * operation.  Re-open and decode the marker so a torn/short write is not
     * used as a recovery checkpoint. */
    RDX_SD0_DIAG_BEGIN(verify_start_ms);
    if (sd0_load_marker(&marker->artifact_id, verify)
        != RDX_STORAGE_RESULT_READY
        || !sd0_marker_equal(marker, verify)) {
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_MARKER_VERIFY, verify_start_ms);
        rdx_sd0_diag_resource("marker_verify_failed", 1);
        rdx_sd0_diag_dump("marker_verify_failed");
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_MARKER_VERIFY, verify_start_ms);
    rdx_sd0_diag_resource("marker_verify_closed", 0);
    if (marker->state == RDX_STORAGE_MARKER_STATE_INDEXED) {
        rdx_sd0_diag_resource("local_stop_complete", 1);
        rdx_sd0_diag_dump("indexed");
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_load_marker(
    const struct rdx_artifact_id *artifact_id,
    struct rdx_storage_marker *marker)
{
    uint8_t *data = rdx_sd0.scratch.marker.data;
    char path[RDX_SD0_PATH_MAX];
    FILE *file;
    uint32_t len;
    uint32_t offset = 0;
    uint32_t stored_crc;

    if (!artifact_id || !artifact_id->value || !marker) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_marker_path(artifact_id, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    file = fopen(path, "r");
    if (!file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    len = flen(file);
    if (len > RDX_SD0_MARKER_DATA_SIZE
        || fread(data, 1, len, file) != (int)len) {
        fclose(file);
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    fclose(file);
    if (len != RDX_SD0_MARKER_DATA_SIZE) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    memset(marker, 0, sizeof(*marker));
    marker->artifact_id.value = sd0_get_u64(data, &offset);
    marker->generation = sd0_get_u32(data, &offset);
    marker->state = sd0_get_u32(data, &offset);
    marker->session_id = sd0_get_u32(data, &offset);
    marker->serial_number = sd0_get_u32(data, &offset);
    marker->produced_frame_count = sd0_get_u32(data, &offset);
    marker->destination_accepted_count = sd0_get_u32(data, &offset);
    marker->local_accepted_count = sd0_get_u32(data, &offset);
    marker->persisted_frame_count = sd0_get_u32(data, &offset);
    marker->rejected_frame_count = sd0_get_u32(data, &offset);
    marker->dropped_frame_count = sd0_get_u32(data, &offset);
    marker->first_error_cause = sd0_get_u32(data, &offset);
    marker->audio_bytes = sd0_get_u64(data, &offset);
    marker->audio_crc = sd0_get_u32(data, &offset);
    marker->marks_count = sd0_get_u32(data, &offset);
    marker->marks_crc = sd0_get_u32(data, &offset);
    memcpy(marker->temp_name, data + offset, sizeof(marker->temp_name));
    offset += sizeof(marker->temp_name);
    memcpy(marker->final_name, data + offset, sizeof(marker->final_name));
    offset += sizeof(marker->final_name);
    stored_crc = sd0_get_u32(data, &offset);
    if (marker->artifact_id.value != artifact_id->value
        || stored_crc != sd0_marker_checksum(data, offset - 4)) {
        return RDX_STORAGE_RESULT_CHECKSUM;
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_load_index(
    struct rdx_storage_index_snapshot *snapshot)
{
    uint8_t *data = rdx_sd0.scratch.index_data;
    uint32_t best_generation = 0;
    uint32_t best_count = 0;
    uint32_t best_schema_version = 0;
    uint32_t best_serial_high_water = 0;
    uint32_t slot;
    uint8_t best_found = 0;
    uint8_t saw_index_file = 0;

    if (!snapshot || !snapshot->entries || !snapshot->capacity) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    snapshot->generation = 0;
    snapshot->schema_version = 0;
    snapshot->serial_high_water = 0;
    snapshot->count = 0;
    for (slot = 0; slot < 2; slot++) {
        char path[RDX_SD0_PATH_MAX];
        FILE *file;
        uint32_t len;
        uint32_t offset = 0;
        uint32_t magic;
        uint32_t version;
        uint32_t generation;
        uint32_t count;
        uint32_t header_size;
        uint32_t serial_high_water = 0;
        uint32_t expected_len;
        uint32_t stored_crc;
        uint32_t i;

        if (snprintf(path, sizeof(path), "%sUXIDX%u.DAT", rdx_sd0.root,
                     (unsigned int)slot) >= sizeof(path)) {
            continue;
        }
        file = fopen(path, "r");
        if (!file) {
            continue;
        }
        saw_index_file = 1;
        len = flen(file);
        if (len > RDX_SD0_INDEX_DATA_SIZE
            || fread(data, 1, len, file) != (int)len) {
            fclose(file);
            continue;
        }
        fclose(file);
        if (len < 20) {
            continue;
        }
        magic = sd0_get_u32(data, &offset);
        version = sd0_get_u32(data, &offset);
        generation = sd0_get_u32(data, &offset);
        count = sd0_get_u32(data, &offset);
        if (magic != RDX_SD0_INDEX_MAGIC
            || (version != RDX_SD0_INDEX_VERSION_V1
                && version != RDX_SD0_INDEX_VERSION)
            || count > RDX_SD0_INDEX_MAX_ENTRIES
            || count > snapshot->capacity) {
            continue;
        }
        if (version == RDX_SD0_INDEX_VERSION) {
            serial_high_water = sd0_get_u32(data, &offset);
            header_size = RDX_SD0_INDEX_HEADER_SIZE;
        } else {
            header_size = RDX_SD0_INDEX_V1_HEADER_SIZE;
        }
        expected_len = header_size
                       + count * RDX_SD0_INDEX_ENTRY_SIZE
                       + RDX_SD0_INDEX_TRAILER_SIZE;
        if (expected_len != len) {
            continue;
        }
        stored_crc = 0;
        offset = len - 4;
        stored_crc = sd0_get_u32(data, &offset);
        if (stored_crc != sd0_marker_checksum(data, len - 4)
            || (best_found && generation <= best_generation)) {
            continue;
        }
        offset = header_size;
        for (i = 0; i < count; i++) {
            struct rdx_storage_file_info *entry =
                &snapshot->entries[i].file;

            memset(entry, 0, sizeof(*entry));
            entry->file_id.artifact_id.value = sd0_get_u64(data, &offset);
            entry->file_id.serial_number = sd0_get_u32(data, &offset);
            if (version == RDX_SD0_INDEX_VERSION_V1
                && entry->file_id.serial_number > serial_high_water) {
                serial_high_water = entry->file_id.serial_number;
            }
            entry->state = sd0_get_u32(data, &offset);
            entry->file_size = sd0_get_u64(data, &offset);
            entry->frame_count = sd0_get_u32(data, &offset);
            entry->audio_crc = sd0_get_u32(data, &offset);
            memcpy(entry->file_id.name, data + offset,
                   sizeof(entry->file_id.name));
            offset += sizeof(entry->file_id.name);
        }
        if (version == RDX_SD0_INDEX_VERSION) {
            for (i = 0; i < count; i++) {
                if (!snapshot->entries[i].file.file_id.serial_number
                    || snapshot->entries[i].file.file_id.serial_number
                       > serial_high_water) {
                    break;
                }
            }
            if (i != count) {
                continue;
            }
        }
        best_generation = generation;
        best_count = count;
        best_schema_version = version;
        best_serial_high_water = serial_high_water;
        best_found = 1;
    }
    snapshot->schema_version = best_schema_version;
    snapshot->generation = best_generation;
    snapshot->serial_high_water = best_serial_high_water;
    snapshot->count = best_count;
    if (saw_index_file && !best_found) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_recover(void)
{
    struct rdx_storage_index_snapshot snapshot;
    uint32_t i;

    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    /* Reuse the backend-owned image; recovery runs before a recording can
     * start and the adapter is single-owner, so no second 32-entry array is
     * needed on the worker stack. */
    memset(rdx_sd0.enumerate_entries, 0,
           sizeof(rdx_sd0.enumerate_entries));
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = rdx_sd0.enumerate_entries;
    snapshot.capacity = RDX_SD0_INDEX_MAX_ENTRIES;
    if (sd0_load_index(&snapshot) != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    if (!snapshot.generation && !snapshot.count) {
        /* New media gets a real, checksummed empty A/B index.  This is a
         * bounded fast path and deliberately does not scan the volume. */
        if (sd0_commit_index(&snapshot) != RDX_STORAGE_RESULT_READY) {
            return RDX_STORAGE_RESULT_IO_ERROR;
        }
        printf("[RDX][LOCAL] index_init schema=%u generation=1"
               " count=0 high_water=0\n", RDX_SD0_INDEX_VERSION);
        return RDX_STORAGE_RESULT_READY;
    }
    /* Recovery is deliberately bounded by the index size.  It never scans
     * the whole FAT volume or guesses that an unindexed file is complete. */
    for (i = 0; i < snapshot.count; i++) {
        struct rdx_storage_marker marker;
        uint64_t file_size;
        enum rdx_storage_result result;

        result = sd0_load_marker(
            &snapshot.entries[i].file.file_id.artifact_id, &marker);
        if (result != RDX_STORAGE_RESULT_READY) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        if (marker.state != RDX_STORAGE_MARKER_STATE_INDEXED
            && marker.state != RDX_STORAGE_MARKER_STATE_PUBLISHED) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        if (snapshot.entries[i].file.state != RDX_ARTIFACT_STATE_COMMITTED
            || marker.serial_number
               != snapshot.entries[i].file.file_id.serial_number
            || strncmp(marker.final_name,
                       snapshot.entries[i].file.file_id.name,
                       sizeof(marker.final_name))) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        result = sd0_file_size(&snapshot.entries[i].file.file_id,
                               &file_size);
        if (result != RDX_STORAGE_RESULT_READY
            || file_size != snapshot.entries[i].file.file_size
            || file_size != marker.audio_bytes
            || snapshot.entries[i].file.audio_crc != marker.audio_crc
            || snapshot.entries[i].file.frame_count
               != marker.persisted_frame_count
            || marker.local_accepted_count
               != marker.persisted_frame_count) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
        if (marker.state == RDX_STORAGE_MARKER_STATE_PUBLISHED) {
            /* A power cut after index commit but before INDEXED marker is
             * recoverable without inventing a second index generation. */
            marker.state = RDX_STORAGE_MARKER_STATE_INDEXED;
            if (sd0_store_marker(&marker) != RDX_STORAGE_RESULT_READY) {
                return RDX_STORAGE_RESULT_CORRUPT;
            }
        }
    }
    if (snapshot.schema_version < RDX_SD0_INDEX_VERSION) {
        /* v1 did not persist the serial high-water separately.  The loader
         * derives it from existing entries, then this bounded A/B commit
         * upgrades the header before any new Artifact can be reserved. */
        if (sd0_commit_index(&snapshot) != RDX_STORAGE_RESULT_READY) {
            return RDX_STORAGE_RESULT_IO_ERROR;
        }
        printf("[RDX][LOCAL] index_upgrade from=%u to=%u generation=%u"
               " count=%u high_water=%u\n",
               (unsigned int)snapshot.schema_version,
               RDX_SD0_INDEX_VERSION,
               (unsigned int)(snapshot.generation + 1),
               (unsigned int)snapshot.count,
               (unsigned int)snapshot.serial_high_water);
    } else {
        printf("[RDX][LOCAL] index_ready schema=%u generation=%u"
               " count=%u high_water=%u\n",
               (unsigned int)snapshot.schema_version,
               (unsigned int)snapshot.generation,
               (unsigned int)snapshot.count,
               (unsigned int)snapshot.serial_high_water);
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_commit_index(
    const struct rdx_storage_index_snapshot *snapshot)
{
    uint8_t *data = rdx_sd0.scratch.index_data;
    char path[RDX_SD0_PATH_MAX];
    FILE *file;
    uint32_t offset = 0;
    uint32_t i;
    uint32_t expected_generation;
    struct rdx_storage_index_snapshot verify;

    if (!snapshot || (!snapshot->entries && snapshot->count)
        || snapshot->count > snapshot->capacity
        || snapshot->count > RDX_SD0_INDEX_MAX_ENTRIES) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (snapshot->generation == UINT32_MAX) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    for (i = 0; i < snapshot->count; i++) {
        if (!snapshot->entries[i].file.file_id.serial_number
            || snapshot->entries[i].file.file_id.serial_number
               > snapshot->serial_high_water) {
            return RDX_STORAGE_RESULT_CORRUPT;
        }
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    sd0_put_u32(data, &offset, RDX_SD0_INDEX_MAGIC);
    sd0_put_u32(data, &offset, RDX_SD0_INDEX_VERSION);
    expected_generation = snapshot->generation + 1;
    sd0_put_u32(data, &offset, expected_generation);
    sd0_put_u32(data, &offset, snapshot->count);
    sd0_put_u32(data, &offset, snapshot->serial_high_water);
    for (i = 0; i < snapshot->count; i++) {
        const struct rdx_storage_file_info *entry =
            &snapshot->entries[i].file;

        sd0_put_u64(data, &offset, entry->file_id.artifact_id.value);
        sd0_put_u32(data, &offset, entry->file_id.serial_number);
        sd0_put_u32(data, &offset, entry->state);
        sd0_put_u64(data, &offset, entry->file_size);
        sd0_put_u32(data, &offset, entry->frame_count);
        sd0_put_u32(data, &offset, entry->audio_crc);
        memcpy(data + offset, entry->file_id.name,
               sizeof(entry->file_id.name));
        offset += sizeof(entry->file_id.name);
    }
    sd0_put_u32(data, &offset, sd0_marker_checksum(data, offset));
    if (snprintf(path, sizeof(path), "%sUXIDX%u.DAT", rdx_sd0.root,
                 (unsigned int)((snapshot->generation + 1) & 1)) >= sizeof(path)) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    RDX_SD0_DIAG_BEGIN(start_ms);
    file = fopen(path, "w+");
    if (!file || fwrite(data, 1, offset, file) != (int)offset) {
        if (file) {
            fclose(file);
        }
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_INDEX_COMMIT, start_ms);
        rdx_sd0_diag_resource("index_commit_failed", 1);
        rdx_sd0_diag_dump("index_commit_failed");
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    fclose(file);
    memset(rdx_sd0.enumerate_entries, 0, sizeof(rdx_sd0.enumerate_entries));
    memset(&verify, 0, sizeof(verify));
    verify.entries = rdx_sd0.enumerate_entries;
    verify.capacity = RDX_SD0_INDEX_MAX_ENTRIES;
    if (sd0_load_index(&verify) != RDX_STORAGE_RESULT_READY
        || verify.schema_version != RDX_SD0_INDEX_VERSION
        || verify.generation != expected_generation
        || verify.serial_high_water != snapshot->serial_high_water
        || verify.count != snapshot->count) {
        RDX_SD0_DIAG_END(RDX_SD0_DIAG_INDEX_COMMIT, start_ms);
        rdx_sd0_diag_resource("index_verify_failed", 1);
        rdx_sd0_diag_dump("index_verify_failed");
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    RDX_SD0_DIAG_END(RDX_SD0_DIAG_INDEX_COMMIT, start_ms);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_quarantine(
    const struct rdx_artifact_id *artifact_id, const char *reason)
{
    char bad_name[RDX_SD0_PATH_MAX];
    const char *base;
    FILE *file;
    int ret;

    (void)reason;
    if (!artifact_id || artifact_id->value != rdx_sd0.open.artifact_id.value) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    if (rdx_sd0.open.file) {
        fclose(rdx_sd0.open.file);
        rdx_sd0.open.file = NULL;
    }
    if (rdx_sd0.open.pending_rename && rdx_sd0.open.temp_path[0]) {
        if (fdelete_by_name(rdx_sd0.open.temp_path)) {
            return RDX_STORAGE_RESULT_IO_ERROR;
        }
        rdx_sd0.open.pending_rename = 0;
        rdx_sd0.open.artifact_id.value = 0;
        return RDX_STORAGE_RESULT_READY;
    }
    /* A published file must be moved to RDXBAD, not silently deleted.  The
     * directory is expected to be provisioned by the product's storage
     * layout; a missing directory is an IO error and remains recoverable. */
    base = rdx_sd0.open.final_path[0]
           ? strrchr(rdx_sd0.open.final_path, '/') : NULL;
    if (!base || snprintf(bad_name, sizeof(bad_name), "RDXBAD/%s",
                          base + 1) >= sizeof(bad_name)) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    file = fopen(rdx_sd0.open.final_path, "r");
    if (!file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    ret = sd0_move_file(file, bad_name);
    if (ret) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    rdx_sd0.open.pending_rename = 0;
    rdx_sd0.open.artifact_id.value = 0;
    return RDX_STORAGE_RESULT_READY;
}

static const struct rdx_storage_media_ops sd0_media_ops = {
    .ensure_ready = sd0_ensure_ready,
    .quiesce = sd0_quiesce,
    .get_capacity = sd0_capacity,
    .open_reader = sd0_open_reader,
    .read_reader = sd0_read_reader,
    .close_reader = sd0_close_reader,
    .stat = sd0_stat,
    .enumerate = sd0_enumerate,
    .delete = sd0_delete,
    .format = sd0_format,
};

static const struct rdx_native_artifact_ops sd0_native_ops = {
    .recover = sd0_recover,
    .open_temp = sd0_open_temp,
    .append = sd0_append,
    .close = sd0_close,
    .truncate = sd0_truncate,
    .publish = sd0_publish,
    .store_marker = sd0_store_marker,
    .load_latest_marker = sd0_load_marker,
    .load_index = sd0_load_index,
    .commit_index = sd0_commit_index,
    .quarantine = sd0_quarantine,
};

int rdx_storage_backend_sd0_jl_br56_init(void)
{
    memset(&rdx_sd0, 0, sizeof(rdx_sd0));
    rdx_sd0.initialized = 1;
    /* Runtime initialization may run on app_core before the Storage Service
     * owns its worker.  Probe only the public device-manager state here;
     * directory checks/creation belong to ensure_ready() on rdx_storage. */
    return sd0_probe_media() == RDX_STORAGE_RESULT_READY ? 0 : -1;
}

const struct rdx_storage_media_ops *
rdx_storage_backend_sd0_jl_br56_media_ops(void)
{
    return &sd0_media_ops;
}

const struct rdx_native_artifact_ops *
rdx_storage_backend_sd0_jl_br56_native_ops(void)
{
    return &sd0_native_ops;
}

#endif /* RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_NATIVE */
