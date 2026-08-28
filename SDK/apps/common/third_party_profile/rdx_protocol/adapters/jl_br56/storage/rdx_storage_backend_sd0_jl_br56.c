#include "app_config.h"

#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_NATIVE

#include <string.h>

#include "dev_manager.h"
#include "fs/fs.h"
#include "rdx_storage_backend_sd0_jl_br56.h"

#define RDX_SD0_PATH_MAX       160
#define RDX_SD0_INDEX_MAGIC    0x30584452UL /* RDX0 */
#define RDX_SD0_INDEX_VERSION  1
#define RDX_SD0_INDEX_MAX_ENTRIES  32u
#define RDX_SD0_INDEX_HEADER_SIZE  16u
#define RDX_SD0_INDEX_ENTRY_SIZE   \
    (8u + 4u + 4u + 8u + 4u + 4u + RDX_STORAGE_NAME_MAX)
#define RDX_SD0_INDEX_TRAILER_SIZE 4u
#define RDX_SD0_INDEX_DATA_SIZE    \
    (RDX_SD0_INDEX_HEADER_SIZE                                     \
     + RDX_SD0_INDEX_MAX_ENTRIES * RDX_SD0_INDEX_ENTRY_SIZE        \
     + RDX_SD0_INDEX_TRAILER_SIZE)
#define RDX_SD0_MARKER_DATA_SIZE   \
    (2u * 8u + 14u * 4u + 2u * RDX_STORAGE_NAME_MAX + 4u)

struct rdx_sd0_open_file {
    FILE *file;
    struct rdx_artifact_id artifact_id;
    char temp_path[RDX_SD0_PATH_MAX];
    char final_path[RDX_SD0_PATH_MAX];
    uint8_t pending_rename;
};

struct rdx_sd0_backend {
    char root[RDX_SD0_PATH_MAX];
    uint8_t initialized;
    struct rdx_sd0_open_file open;
    /* The backend is single-owner. Keep the maximum index image off the
     * storage worker stack and reuse it for A/B reads and commits. */
    uint8_t index_data[RDX_SD0_INDEX_DATA_SIZE];
    struct rdx_storage_index_entry enumerate_entries[RDX_SD0_INDEX_MAX_ENTRIES];
};

static struct rdx_sd0_backend rdx_sd0;

static enum rdx_storage_result sd0_load_index(
    struct rdx_storage_index_snapshot *snapshot);

static enum rdx_storage_result sd0_ready(void)
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
    *total_bytes = (uint64_t)part->total_size;
    *free_bytes = (uint64_t)free_kb * 1024ULL;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_read(
    const struct rdx_storage_file_id *file_id, uint64_t offset,
    uint8_t *buf, uint32_t len, uint32_t *read_len)
{
    FILE *file;
    char path[RDX_SD0_PATH_MAX];
    int count;

    if (!file_id || !buf || !read_len || offset > 0xffffffffULL) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_path(file_id->name, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    file = fopen(path, "r");
    if (!file || fseek(file, (u32)offset, SEEK_SET)) {
        if (file) {
            fclose(file);
        }
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    count = fread(buf, 1, len, file);
    fclose(file);
    if (count < 0) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    *read_len = (uint32_t)count;
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
    *file_size = flen(file);
    *audio_crc = 0; /* CRC is persisted in the Artifact marker. */
    fclose(file);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_enumerate(
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count)
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

    if (!file_id) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_path(file_id->name, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    return fdelete_by_name(path) ? RDX_STORAGE_RESULT_IO_ERROR
                                 : RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_format(void)
{
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    return f_format(rdx_sd0.root, "fat", 0) ? RDX_STORAGE_RESULT_IO_ERROR
                                            : RDX_STORAGE_RESULT_READY;
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
    open->file = fopen(open->temp_path, "w+");
    if (!open->file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    open->artifact_id = spec->artifact_id;
    open->pending_rename = 1;
    *handle = open;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_append(void *handle, const uint8_t *data,
                                          uint32_t len, uint32_t *written)
{
    struct rdx_sd0_open_file *open = handle;
    int count;

    if (!open || !open->file || !data || !written) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    count = fwrite((void *)data, 1, len, open->file);
    if (count < 0) {
        *written = 0;
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    *written = (uint32_t)count;
    return *written == len ? RDX_STORAGE_RESULT_READY
                           : RDX_STORAGE_RESULT_IO_ERROR;
}

static enum rdx_storage_result sd0_close(void *handle, uint64_t *file_size,
                                         uint32_t *audio_crc)
{
    struct rdx_sd0_open_file *open = handle;

    if (!open || !open->file || !file_size || !audio_crc) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    *file_size = flen(open->file);
    *audio_crc = 0;
    fclose(open->file);
    open->file = NULL;
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

static enum rdx_storage_result sd0_publish(const char *temp_name,
                                           const char *final_name)
{
    struct rdx_sd0_open_file *open = &rdx_sd0.open;
    char temp_path[RDX_SD0_PATH_MAX];
    char final_path[RDX_SD0_PATH_MAX];
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
    open->file = fopen(open->temp_path, "r");
    if (!open->file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    ret = frename(open->file, final_path);
    fclose(open->file);
    open->file = NULL;
    if (!ret) {
        open->pending_rename = 0;
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

static enum rdx_storage_result sd0_store_marker(
    const struct rdx_storage_marker *marker)
{
    uint8_t data[RDX_SD0_MARKER_DATA_SIZE];
    char path[RDX_SD0_PATH_MAX];
    FILE *file;
    uint32_t len;

    if (!marker) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    if (sd0_marker_path(&marker->artifact_id, path, sizeof(path))) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    len = sd0_serialize_marker(marker, data, sizeof(data));
    if (len != sizeof(data)) {
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    file = fopen(path, "w+");
    if (!file || fwrite(data, 1, len, file) != (int)len) {
        if (file) {
            fclose(file);
        }
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    fclose(file);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_load_marker(
    const struct rdx_artifact_id *artifact_id,
    struct rdx_storage_marker *marker)
{
    uint8_t data[RDX_SD0_MARKER_DATA_SIZE];
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
    if (len > sizeof(data) || fread(data, 1, len, file) != (int)len) {
        fclose(file);
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    fclose(file);
    if (len != sizeof(data)) {
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
    uint8_t *data = rdx_sd0.index_data;
    uint32_t best_generation = 0;
    uint32_t best_count = 0;
    uint32_t slot;
    uint8_t best_found = 0;

    if (!snapshot || !snapshot->entries || !snapshot->capacity) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    snapshot->generation = 0;
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
        if (magic != RDX_SD0_INDEX_MAGIC || version != RDX_SD0_INDEX_VERSION
            || count > RDX_SD0_INDEX_MAX_ENTRIES
            || count > snapshot->capacity) {
            continue;
        }
        expected_len = RDX_SD0_INDEX_HEADER_SIZE
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
        offset = RDX_SD0_INDEX_HEADER_SIZE;
        for (i = 0; i < count; i++) {
            struct rdx_storage_file_info *entry =
                &snapshot->entries[i].file;

            memset(entry, 0, sizeof(*entry));
            entry->file_id.artifact_id.value = sd0_get_u64(data, &offset);
            entry->file_id.serial_number = sd0_get_u32(data, &offset);
            entry->state = sd0_get_u32(data, &offset);
            entry->file_size = sd0_get_u64(data, &offset);
            entry->frame_count = sd0_get_u32(data, &offset);
            entry->audio_crc = sd0_get_u32(data, &offset);
            memcpy(entry->file_id.name, data + offset,
                   sizeof(entry->file_id.name));
            offset += sizeof(entry->file_id.name);
        }
        best_generation = generation;
        best_count = count;
        best_found = 1;
    }
    snapshot->generation = best_generation;
    snapshot->count = best_count;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_commit_index(
    const struct rdx_storage_index_snapshot *snapshot)
{
    uint8_t *data = rdx_sd0.index_data;
    char path[RDX_SD0_PATH_MAX];
    FILE *file;
    uint32_t offset = 0;
    uint32_t i;

    if (!snapshot || (!snapshot->entries && snapshot->count)
        || snapshot->count > snapshot->capacity
        || snapshot->count > RDX_SD0_INDEX_MAX_ENTRIES) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (sd0_ready() != RDX_STORAGE_RESULT_READY) {
        return RDX_STORAGE_RESULT_NOT_READY;
    }
    sd0_put_u32(data, &offset, RDX_SD0_INDEX_MAGIC);
    sd0_put_u32(data, &offset, RDX_SD0_INDEX_VERSION);
    sd0_put_u32(data, &offset, snapshot->generation + 1);
    sd0_put_u32(data, &offset, snapshot->count);
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
    file = fopen(path, "w+");
    if (!file || fwrite(data, 1, offset, file) != (int)offset) {
        if (file) {
            fclose(file);
        }
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    fclose(file);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result sd0_quarantine(
    const struct rdx_artifact_id *artifact_id, const char *reason)
{
    char bad_path[RDX_SD0_PATH_MAX];
    const char *base;
    FILE *file;

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
    if (!base || snprintf(bad_path, sizeof(bad_path), "%sRDXBAD/%s",
                          rdx_sd0.root, base + 1) >= sizeof(bad_path)) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    file = fopen(rdx_sd0.open.final_path, "r");
    if (!file) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    if (frename(file, bad_path)) {
        fclose(file);
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    fclose(file);
    rdx_sd0.open.pending_rename = 0;
    rdx_sd0.open.artifact_id.value = 0;
    return RDX_STORAGE_RESULT_READY;
}

static const struct rdx_storage_media_ops sd0_media_ops = {
    .ensure_ready = sd0_ensure_ready,
    .quiesce = sd0_quiesce,
    .get_capacity = sd0_capacity,
    .read = sd0_read,
    .stat = sd0_stat,
    .enumerate = sd0_enumerate,
    .delete = sd0_delete,
    .format = sd0_format,
};

static const struct rdx_native_artifact_ops sd0_native_ops = {
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
    return sd0_ready() == RDX_STORAGE_RESULT_READY ? 0 : -1;
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
