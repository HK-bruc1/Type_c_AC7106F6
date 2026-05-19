#ifndef _LHDC_X_NODE_H_
#define _LHDC_X_NODE_H_

#include "generic/typedef.h"

struct lhdc_x_param_tool_set {
    int is_bypass;
};

typedef enum __LHDCX_FUNC_RET__ {
    LHDCX_FRET_SUCCESS = 0,
    LHDCX_FRET_ERROR = -1,
    LHDCX_FRET_INVALID_INPUT_PARAM = -2,
    LHDCX_FRET_INVALID_HANDLE = -3,
    LHDCX_FRET_ERROR_RUN_1 = -4,
    LHDCX_FRET_ERROR_RUN_2 = -5,
    LHDCX_FRET_INVALID_FUNCTION = -6,
} LHDCX_FUNC_RET;

typedef struct LhdcxInSplitChannel {
    void *ch_l;
    void *ch_r;
} LhdcxInSplitChannel;

enum lhdcx_pcm_format {
    LHDCX_PCM_FORMAT_S16 = 0,
    LHDCX_PCM_FORMAT_S24,
    LHDCX_FLOAT_FORMAT,
    LHDCX_PCM_FORMAT_S24_HIGH_SPLIT,
};

enum lhdcx_out_channel {
    LHDCX_OUT_STEREO = 0,
    LHDCX_OUT_LEFT_CHANNEL_ONLY,
    LHDCX_OUT_RIGHT_CHANNEL_ONLY,
};

/** lhdcx_util_get_version
 *  Return version string
 *
 *  parameter:
 *    none
 *  return:
 *    version string
 */
char *lhdcx_util_get_version(void);


/** lhdcx_util_get_mem_size
 *  Return byte size of memory needed for an encoder (for dynamic memory allocation).
 *
 *  parameter:
 *    size, return size of bytes
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_get_mem_size(int *size);

/** lhdcx_util_init
 *  Setup lhdc x edge
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    sample, maximun sample per frame
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_init(void *handle, int sample_rate, enum lhdcx_pcm_format fmt);

/** lhdcx_util_process
 *  process a frame
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    in, pointer to the PCM stream input buffer. Must be stereo interleaved data.
 *    fmt, PCM input format (bit-per-sample) index. Support 16 bits per sample and 24 bits per sample.
 *    out, pointer to the PCM stream output buffer. Must be stereo interleaved data.
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_process(void *handle, const void *in, int samples, void *out);

/** lhdcx_util_set_out_channel
 *  For mono devices only one channel of output needs to be processed. Set channels to control process channels.
 *  Default is stereo. (output PCM format will not be changed)
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    out_channel, process channel setting
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_set_out_channel(void *handle, enum lhdcx_out_channel out_channel);

/** lhdcx_util_get_factor_bytes
 *  Get factor bytes size
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    bytes, the bytes size of the factor struct
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_get_factor_bytes(void *handle, int *bytes);

/** lhdcx_util_set_factor
 *  set factor array to LHDC-X-Edge. This fucntion must be executed before processing.
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    factor, pointer to control factor array
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_set_factor(void *handle, const void *factor);

/** lhdcx_util_set_factor
 *  Get current factor in handle context.
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    factor, pointer to control factor array
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_get_factor(void *handle, const void *factor);

/** lhdcx_util_set_angle
 *  Set LHDC-X-Edge angle. Include horizontal angle and elevation angle
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *    h_angle, horizontal andle. [Min, Max]=[0,360]
 *    e_angle, elevation angle. [Min, Max]=[-60,60]
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_set_angle(void *handle, float h_angle, float e_angle);

/** lhdcx_util_reset_cache_buffer
 *  Clear LHDC-X-Edge audio buffer without reinitialization
 *
 *  parameter:
 *    handle, handle of the lhdcx context
 *  return:
 *    Success, 0
 *    Fail, value lesser than 0
 */
int lhdcx_util_reset_cache_buffer(void *handle);

#endif
