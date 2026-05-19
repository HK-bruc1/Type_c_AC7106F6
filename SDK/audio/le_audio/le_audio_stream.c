/*************************************************************************************************/
/*!
*  \file      le_audio_stream.c
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#include "generic/atomic.h"
#include "le_audio_stream.h"
#include "audio_base.h"
#include "circular_buf.h"
#include "system/timer.h"
#include "app_config.h"
#include "audio_time.h"
#include "media_config.h"
#include "wireless_trans.h"
#include "tech_lib/jla_ll_codec_api.h"
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
#include "surround_sound.h"
#endif

#if LE_AUDIO_STREAM_ENABLE

#define LE_AUDIO_TX_TEST        0

//le_audio tx/rx buf size 配置, 配置可以容纳的编码帧的个数,通过isoIntervalUs_len转换成具体的size
#define LE_AUDIO_TX_BUF_CONTAIN_FREAM_NUMBER       6
#define LE_AUDIO_RX_BUF_CONTAIN_FREAM_NUMBER       10
#define LE_AUDIO_RX_BUF_CONTAIN_FREAM_NUMBER_PCM   3   //本地使用pcm格式同步播放，申请的buf配置.

struct le_audio_stream_buf {
    void *addr;
    int size;
    cbuffer_t cbuf;
};

struct le_audio_tx_stream {
    struct le_audio_stream_buf buf;
    void *tick_priv;
    int (*tick_handler)(void *priv, int len, u32 timestamp);
    void *parent;
    u32 coding_type;
    u16 frame_size;

#if LE_AUDIO_TX_TEST
    u16 test_timer;
    int isoIntervalUs_len;
    void *test_buf;
#endif
};

struct le_audio_rx_stream {
    struct le_audio_stream_buf buf;
    struct list_head frames;
    int isoIntervalUs_len;
    int frames_len;
    int frames_max_size;
    u32 coding_type;
    atomic_t ref;
    void *parent;
    u32 timestamp;
    u16 fill_data_timer;
    u8 online;
};

struct le_audio_stream_context {
    u16 conn;
    u8 start;
    u32 start_time;
    struct le_audio_stream_format fmt;
    struct le_audio_tx_stream *tx_stream;
    struct le_audio_rx_stream *rx_stream;
    spinlock_t lock;
    void *tx_tick_priv;
    int (*tx_tick_handler)(void *priv, int period, u32 timestamp);
    void *rx_tick_priv;
    void (*rx_tick_handler)(void *priv);
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
    struct le_audio_tx_stream *tx_stream_2nd;
    void *tx_tick_priv_2nd;
    int (*tx_tick_handler_2nd)(void *priv, int period, u32 timestamp);
#endif
};

extern int bt_audio_reference_clock_select(void *addr, u8 network);
extern u32 bb_le_clk_get_time_us(void);
extern void ll_config_ctrler_clk(uint16_t handle, uint8_t sel);

int le_audio_get_encoder_len(u32 coding_type, u16 frame_len, u32 bit_rate)
{
    int len = 0;
    switch (coding_type) {
    case AUDIO_CODING_LC3:
        len = (frame_len * bit_rate / 1000 / 8 / 10);
        break;
    case AUDIO_CODING_JLA:
        len = (frame_len * bit_rate / 1000 / 8 / 10 + 2);
#if JL_CC_CODED_EN
        len += (len & 1); //开fec 编码时会微调码率，使编出来一帧的数据长度是偶数,故如果计算出来是奇数需要+1;
#endif/*JL_CC_CODED_EN*/
        break;
    case AUDIO_CODING_JLA_V2:
        len = (frame_len * bit_rate / 1000 / 8 / 10 + (JLA_V2_CODEC_WITH_FRAME_HEADER == 0 ? 0 : 2));
        break;
#if (LE_AUDIO_CODEC_TYPE == AUDIO_CODING_JLA_LL)
    case AUDIO_CODING_JLA_LL:
        len = jla_ll_enc_frame_len();
        break;
#endif
    case AUDIO_CODING_JLA_LW:
        len = (frame_len * bit_rate / 1000 / 8 / 10);
        break;
    default:
        printf(" %s, %d, unsuport this type: 0x%x \n", __func__, __LINE__, coding_type);
        break;
    }
    /* printf(" le_audio_encoder_len:   coding_type: 0x%x  encoder_len: %d\n",coding_type,len); */
    return len;
}



void *le_audio_stream_create(u16 conn, struct le_audio_stream_format *fmt)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)zalloc(sizeof(struct le_audio_stream_context));

    memcpy(&ctx->fmt, fmt, sizeof(ctx->fmt));

    printf("[le audio stream : %d, %d, 0x%x, %d, %d, %d]\n", ctx->fmt.nch, ctx->fmt.bit_rate, ctx->fmt.coding_type,
           ctx->fmt.frame_dms, ctx->fmt.isoIntervalUs, ctx->fmt.sample_rate);
    spin_lock_init(&ctx->lock);
    ctx->conn = conn;
    ASSERT(ctx->conn != 0, "conn is NULL; le_audio sync will not work !!!\n");

    ctx->start = 1;

    return ctx;
}

void le_audio_stream_free(void *le_audio)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    if (ctx) {
        free(ctx);
    }
}

int le_audio_stream_clock_select(void *le_audio)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    bt_audio_reference_clock_select(NULL, 2);
    ll_config_ctrler_clk((uint16_t)ctx->conn, 0); //蓝牙与同步关联 bis_hdl/cis_hdl
    return 0;
}

u32 le_audio_stream_current_time(void *le_audio)
{
    return bb_le_clk_get_time_us();
}

#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
static int __le_audio_stream_dual_tx_data_handler(void *_ctx, void *data, int len, u32 timestamp, int latency)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)_ctx;
    struct le_audio_tx_stream *tx_stream = ctx->tx_stream;
    struct le_audio_tx_stream *tx_stream_2nd = ctx->tx_stream_2nd;

    struct le_audio_rx_stream *rx_stream = ctx->rx_stream;
    u32 rlen = 0;
    u32 r_len = 0;
    /* printf("len :%d\n",len); */
    /*putchar('A');*/
    if (!ctx->start) {
        u32 time_diff = (bb_le_clk_get_time_us() - ctx->start_time) & 0xfffffff;
        if (time_diff > 5000000) {
            return 0;
        }
        ctx->start = 1;
    }

    if (((cbuf_get_data_len(&tx_stream->buf.cbuf) < tx_stream->frame_size) || (cbuf_get_data_len(&tx_stream_2nd->buf.cbuf) < tx_stream_2nd->frame_size)) ||
        (rx_stream && cbuf_get_data_len(&rx_stream->buf.cbuf) < rx_stream->isoIntervalUs_len)) {
        /*对于需要本地播放的必须满足播放与发送都有一个interval的数据*/
        y_printf("no data : %d, %d, %d, %d, %d\n", cbuf_get_data_len(&tx_stream->buf.cbuf), cbuf_get_data_len(&tx_stream_2nd->buf.cbuf), len, tx_stream->frame_size, tx_stream_2nd->frame_size);
        return 0;
    }

    spin_lock(&ctx->lock);
    rlen = cbuf_read(&tx_stream->buf.cbuf, data, tx_stream->frame_size);
    r_len = cbuf_read(&tx_stream_2nd->buf.cbuf, (u8 *)data + rlen, tx_stream_2nd->frame_size);
    rlen += r_len;
    spin_unlock(&ctx->lock);
    if (!rlen) {
        return 0;
    }

    if (ctx->tx_tick_handler) {
        ctx->tx_tick_handler(ctx->tx_tick_priv, ctx->fmt.isoIntervalUs, timestamp);
    }
    if (ctx->tx_tick_handler_2nd) {
        ctx->tx_tick_handler_2nd(ctx->tx_tick_priv_2nd, ctx->fmt.isoIntervalUs, timestamp);
    }

    if (tx_stream->tick_handler) {
        tx_stream->tick_handler(tx_stream->tick_priv, tx_stream->frame_size, timestamp);
    }
    if (tx_stream_2nd->tick_handler) {
        tx_stream_2nd->tick_handler(tx_stream->tick_priv, tx_stream_2nd->frame_size, timestamp);
    }
    return rlen;
}

#endif

static int __le_audio_stream_tx_data_handler(void *stream, void *data, int len, u32 timestamp, int latency)
{
    struct le_audio_tx_stream *tx_stream = (struct le_audio_tx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)tx_stream->parent;
    u32 rlen = 0;

#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
    rlen = __le_audio_stream_dual_tx_data_handler(ctx, data, len, timestamp, latency);
#else
    struct le_audio_rx_stream *rx_stream = ctx->rx_stream;
    u32 read_alloc_len = 0;
    /*putchar('A');*/
    if (!ctx->start) {
        u32 time_diff = (bb_le_clk_get_time_us() - ctx->start_time) & 0xfffffff;
        if (time_diff > 5000000) {
            return 0;
        }
        ctx->start = 1;
    }

    if (cbuf_get_data_len(&tx_stream->buf.cbuf) < len ||
        (rx_stream && cbuf_get_data_len(&rx_stream->buf.cbuf) < rx_stream->isoIntervalUs_len)) {
        /*对于需要本地播放的必须满足播放与发送都有一个interval的数据*/
        /*y_printf("no data : %u, %d\n", timestamp, latency);*/
        return 0;
    }

    spin_lock(&ctx->lock);
    rlen = cbuf_read(&tx_stream->buf.cbuf, data, len);
    spin_unlock(&ctx->lock);
    /*printf("-%d, %d-\n", len, rlen);*/
    if (!rlen) {
        return 0;
    }

    if (ctx->tx_tick_handler) {
        ctx->tx_tick_handler(ctx->tx_tick_priv, ctx->fmt.isoIntervalUs, timestamp);
    }

    if (tx_stream->tick_handler) {
        tx_stream->tick_handler(tx_stream->tick_priv, len, timestamp);
    }

    /*putchar('B');*/
    if (rx_stream) {
        spin_lock(&ctx->lock);
        void *addr = cbuf_read_alloc(&rx_stream->buf.cbuf, &read_alloc_len);
        if (read_alloc_len < rx_stream->isoIntervalUs_len) {
            printf("local not align to tx.\n");
            spin_unlock(&ctx->lock);
            return rlen;
        }
        if ((tx_stream->coding_type == AUDIO_CODING_LC3 || tx_stream->coding_type == AUDIO_CODING_JLA) &&
            rx_stream->coding_type == AUDIO_CODING_PCM) {
            timestamp = (timestamp + (ctx->fmt.frame_dms == 75 ? 4000L : 2500L)) & 0xfffffff;
#if (LE_AUDIO_CODEC_TYPE == AUDIO_CODING_JLA_V2)
        } else if (tx_stream->coding_type == AUDIO_CODING_JLA_V2 && rx_stream->coding_type == AUDIO_CODING_PCM) {
            u16 input_point = ctx->fmt.frame_dms * ctx->fmt.sample_rate / 1000 / 10;
            u32 time_diff = 0;
            if (!(JLA_V2_FRAMELEN_MASK & 0x8000) || input_point >= 160) { //最高位是0或者点数大于160,jla_v2的延时是1/4帧
                time_diff = ctx->fmt.frame_dms * 1000 / 10 /  4; //(us)
            } else {
                time_diff = ctx->fmt.frame_dms * 1000 / 10; //(us)
            }
            timestamp = (timestamp + time_diff) & 0xfffffff;
#endif
        }
        timestamp = (timestamp + latency) & 0xfffffff;

        //对于sdu_interval 比较大的时，按照sdu_interal填数据，可以节省ram
        if ((ctx->fmt.isoIntervalUs != ctx->fmt.sdu_period) && (ctx->fmt.sdu_period == 10 * 1000)) { //这里默认10ms 的编码帧才这样处理
            int sdu_num = ctx->fmt.isoIntervalUs / ctx->fmt.sdu_period;
            int sduIntervalUs_len = rx_stream->isoIntervalUs_len / sdu_num;
            u8 *tmp_data = (u8 *)addr;
            for (int i = 0; i < sdu_num; i++) {
                /* printf(" %d, %d, %d, %u\n",sdu_num,sduIntervalUs_len , rx_stream->isoIntervalUs_len,timestamp); */
                le_audio_stream_rx_frame(rx_stream, tmp_data, sduIntervalUs_len, timestamp);
                timestamp = (timestamp + ctx->fmt.sdu_period) & 0xfffffff;
                tmp_data += sduIntervalUs_len;
            }

        } else {
            le_audio_stream_rx_frame(rx_stream, addr, rx_stream->isoIntervalUs_len, timestamp);
        }

        cbuf_read_updata(&rx_stream->buf.cbuf, rx_stream->isoIntervalUs_len);
        spin_unlock(&ctx->lock);
        /*printf("-%d-\n", rx_stream->isoIntervalUs_len);*/
    }
#endif
    return rlen;
}

int le_audio_stream_tx_data_handler(void *le_audio, void *data, int len, u32 timestamp, int latency)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    spin_lock(&ctx->lock);
    if (ctx->tx_stream) {
        int rlen = __le_audio_stream_tx_data_handler(ctx->tx_stream, data, len, timestamp, latency);
        spin_unlock(&ctx->lock);
        return rlen;

    }
    spin_unlock(&ctx->lock);

    return 0;
}

#if LE_AUDIO_TX_TEST
static void le_audio_tx_test_timer(void *stream)
{
    struct le_audio_tx_stream *tx_stream = (struct le_audio_tx_stream *)stream;

    __le_audio_stream_tx_data_handler(tx_stream, tx_stream->test_buf, tx_stream->isoIntervalUs_len, 0x12345678);
}
#endif

void le_audio_stream_set_tx_tick_handler(void *le_audio, void *priv, int (*tick_hanlder)(void *, int, u32), u8 ch)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    if (!ctx) {
        return;
    }

    spin_lock(&ctx->lock);

    if (ch) {
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
        ctx->tx_tick_handler_2nd = tick_hanlder;
        ctx->tx_tick_priv_2nd = priv;
#endif
    } else {
        ctx->tx_tick_handler = tick_hanlder;
        ctx->tx_tick_priv = priv;
    }
    spin_unlock(&ctx->lock);
}

#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
void *le_audio_dual_stream_tx_open(void *le_audio, int coding_type, int frame_size, u8 ch)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_tx_stream *tx_stream = NULL;

    tx_stream = (struct le_audio_tx_stream *)zalloc(sizeof(struct le_audio_tx_stream));


    int isoIntervalUs_len = (ctx->fmt.isoIntervalUs / 100 / ctx->fmt.frame_dms) * frame_size;
    tx_stream->buf.size = isoIntervalUs_len * LE_AUDIO_TX_BUF_CONTAIN_FREAM_NUMBER;
    tx_stream->buf.addr = malloc(tx_stream->buf.size);
    ASSERT(tx_stream->buf.addr != NULL, "please check audio param");
    printf("tx stream buffer : 0x%x, %d\n", (u32)tx_stream->buf.addr, tx_stream->buf.size);
    cbuf_init(&tx_stream->buf.cbuf, tx_stream->buf.addr, tx_stream->buf.size);
    tx_stream->coding_type = coding_type;
    tx_stream->parent = ctx;
    tx_stream->tick_priv = NULL;
    tx_stream->tick_handler = NULL;
    tx_stream->frame_size = frame_size;

    if (ch) {
        ctx->tx_stream_2nd = tx_stream;
        /* y_printf("== ctx->tx_stream_2nd, 0x%x\n",(int)ctx->tx_stream_2nd); */
    } else {
        ctx->tx_stream = tx_stream;
        /* y_printf("== ctx->tx_stream, 0x%x\n",(int)ctx->tx_stream); */
    }

    return tx_stream;
}
#endif

void *le_audio_stream_tx_open(void *le_audio, int coding_type, void *priv, int (*tick_handler)(void *, int, u32))
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_tx_stream *tx_stream = NULL;
    int frame_size = 0;

    if (ctx->tx_stream) {
        return ctx->tx_stream;
    }

    tx_stream = (struct le_audio_tx_stream *)zalloc(sizeof(struct le_audio_tx_stream));
    frame_size = le_audio_get_encoder_len(ctx->fmt.coding_type, ctx->fmt.frame_dms, ctx->fmt.bit_rate);
    int isoIntervalUs_len = (ctx->fmt.isoIntervalUs / 100 / ctx->fmt.frame_dms) * frame_size;
    tx_stream->buf.size = isoIntervalUs_len * LE_AUDIO_TX_BUF_CONTAIN_FREAM_NUMBER;
    tx_stream->buf.addr = malloc(tx_stream->buf.size);
    ASSERT(tx_stream->buf.addr != NULL, "please check audio param");
    printf("tx stream buffer : 0x%x, %d\n", (u32)tx_stream->buf.addr, tx_stream->buf.size);
    cbuf_init(&tx_stream->buf.cbuf, tx_stream->buf.addr, tx_stream->buf.size);
    tx_stream->coding_type = coding_type;
    tx_stream->parent = ctx;
    tx_stream->tick_priv = priv;
    tx_stream->tick_handler = tick_handler;

    ctx->tx_stream = tx_stream;

#if LE_AUDIO_TX_TEST
    tx_stream->test_timer = sys_hi_timer_add(tx_stream, le_audio_tx_test_timer, ctx->fmt.isoIntervalUs / 1000);
    tx_stream->test_buf = malloc(isoIntervalUs_len);
    tx_stream->isoIntervalUs_len = isoIntervalUs_len;
#else
    /* le_audio_set_tx_data_handler(ctx->conn, tx_stream, le_audio_stream_tx_data_handler); */
#endif

    return tx_stream;
}

void le_audio_stream_tx_close(void *stream)
{
    struct le_audio_tx_stream *tx_stream = (struct le_audio_tx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)tx_stream->parent;

    spin_lock(&ctx->lock);
#if LE_AUDIO_TX_TEST
    if (tx_stream->test_timer) {
        sys_hi_timer_del(tx_stream->test_timer);
    }
    if (tx_stream->test_buf) {
        free(tx_stream->test_buf);
    }
#endif
    if (tx_stream->buf.addr) {
        free(tx_stream->buf.addr);
    }

    free(tx_stream);
    ctx->tx_stream = NULL;
    spin_unlock(&ctx->lock);
}

int le_audio_stream_tx_buffered_time(void *stream)
{
    struct le_audio_tx_stream *tx_stream = (struct le_audio_tx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)tx_stream->parent;

    int frame_size = le_audio_get_encoder_len(ctx->fmt.coding_type, ctx->fmt.frame_dms, ctx->fmt.bit_rate);

    return cbuf_get_data_len(&tx_stream->buf.cbuf) / frame_size * ctx->fmt.frame_dms * 100;
}

int le_audio_stream_set_start_time(void *le_audio, u32 start_time)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    if (ctx) {
        ctx->start_time = start_time;
        ctx->start = 0;
    }

    return 0;
}

int le_audio_stream_set_bit_width(void *le_audio, u8 bit_width)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    if (ctx) {
        ctx->fmt.bit_width = bit_width;
        return 0;
    }
    return -1;
}

int le_audio_stream_rx_buf_init(void *le_audio, int coding_type)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_rx_stream *rx_stream = ctx->rx_stream;
    int frame_size = 0;
    int buf_frame_number = 0;

    if (!rx_stream ||  rx_stream->frames_max_size) {
        return -1;
    }

    if (coding_type == AUDIO_CODING_PCM) {
        buf_frame_number = LE_AUDIO_RX_BUF_CONTAIN_FREAM_NUMBER_PCM;
        frame_size = ctx->fmt.frame_dms * ctx->fmt.sample_rate * ctx->fmt.nch * (ctx->fmt.bit_width ? 4 : 2) / 10000;
    } else {
        buf_frame_number = LE_AUDIO_RX_BUF_CONTAIN_FREAM_NUMBER;
        frame_size = le_audio_get_encoder_len(coding_type, ctx->fmt.frame_dms, ctx->fmt.bit_rate);
    }

    int iso_interval_len = (ctx->fmt.isoIntervalUs / 100 / ctx->fmt.frame_dms) * frame_size;
    /*如果存在flush timeout，那么缓冲需要大于flush timeout的数量*/
    rx_stream->frames_max_size = iso_interval_len * (ctx->fmt.flush_timeout ? (ctx->fmt.flush_timeout + 5) : buf_frame_number);
    printf("rx_stream->frames_max_size: %d, iso_interval_len : %d, frame_size : %d, \n", rx_stream->frames_max_size, iso_interval_len, frame_size);
    rx_stream->buf.size = rx_stream->frames_max_size;
    rx_stream->buf.addr = malloc(rx_stream->frames_max_size);
    rx_stream->isoIntervalUs_len = iso_interval_len;
    cbuf_init(&rx_stream->buf.cbuf, rx_stream->buf.addr, rx_stream->buf.size);

    return 0;
}


void *le_audio_stream_rx_open(void *le_audio, int coding_type)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_rx_stream *rx_stream = ctx->rx_stream;

    if (rx_stream) {
        atomic_inc(&rx_stream->ref);
        return rx_stream;
    }

    rx_stream = (struct le_audio_rx_stream *)zalloc(sizeof(struct le_audio_rx_stream));
    if (!rx_stream) {
        return NULL;
    }
    INIT_LIST_HEAD(&rx_stream->frames);

    rx_stream->parent = ctx;
    rx_stream->coding_type = coding_type;
    rx_stream->online = 1;

    atomic_inc(&rx_stream->ref);
    ctx->rx_stream = rx_stream;

    //解码非pcm 格式，可以提前初始化buf,pcm格式需要等设置完位宽参数，才能初始化buf
    if (coding_type != AUDIO_CODING_PCM) {
        le_audio_stream_rx_buf_init(ctx, coding_type);
    }

    return rx_stream;
}

void le_audio_stream_rx_close(void *stream)
{
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)rx_stream->parent;

    if (!rx_stream) {
        return;
    }

    spin_lock(&ctx->lock);
    if (atomic_dec(&rx_stream->ref) == 0) {
        if (rx_stream->fill_data_timer) {
            sys_hi_timer_del(rx_stream->fill_data_timer);
        }
        if (rx_stream->buf.addr) {
            free(rx_stream->buf.addr);
        }
        struct le_audio_frame *frame, *n;
        list_for_each_entry_safe(frame, n, &rx_stream->frames, entry) {
            __list_del_entry(&frame->entry);
            free(frame);
        }
        free(rx_stream);
        ctx->rx_stream = NULL;
    }
    spin_unlock(&ctx->lock);
}

int le_audio_stream_get_rx_format(void *le_audio, struct le_audio_stream_format *fmt)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)ctx->rx_stream;

    if (!rx_stream) {
        return -EINVAL;
    }
    fmt->coding_type = rx_stream->coding_type;
    fmt->nch = ctx->fmt.nch;
    fmt->sample_rate = ctx->fmt.sample_rate;
    fmt->frame_dms = ctx->fmt.frame_dms;
    fmt->bit_rate = ctx->fmt.bit_rate;
    //y_printf("le_audio param : %x,%d,%d,%d,%d\n", fmt->coding_type, fmt->nch, fmt->sample_rate, fmt->frame_dms, fmt->bit_rate);

    return 0;
}

int le_audio_stream_get_dec_ch_mode(void *le_audio)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    if (!ctx) {
        return -EINVAL;
    }

    return ctx->fmt.dec_ch_mode;
}

void le_audio_stream_set_rx_tick_handler(void *le_audio, void *priv, void (*tick_handler)(void *priv))
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;

    if (ctx) {
        spin_lock(&ctx->lock);
        ctx->rx_tick_priv = priv;
        ctx->rx_tick_handler = tick_handler;
        spin_unlock(&ctx->lock);
    }
}

int le_audio_stream_tx_write(void *stream, void *data, int len)
{
    struct le_audio_tx_stream *tx_stream = (struct le_audio_tx_stream *)stream;

    int wlen = cbuf_write(&tx_stream->buf.cbuf, (u8 *)data, len);
    if (wlen < len) {
        /* putchar('t'); */
    }

    return wlen;
}

int le_audio_stream_rx_write(void *stream, void *data, int len)
{
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)stream;

    int wlen = cbuf_write(&rx_stream->buf.cbuf, data, len);
    if (wlen < len) {
        /* putchar('r'); */
    }

    return wlen;
}

int le_audio_stream_tx_drain(void *stream)
{
    struct le_audio_tx_stream *tx_stream = (struct le_audio_tx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)tx_stream->parent;

    /*r_printf("tx buffered : %d\n", cbuf_get_data_len(&tx_stream->buf.cbuf));*/
    spin_lock(&ctx->lock);
    cbuf_clear(&tx_stream->buf.cbuf);
    spin_unlock(&ctx->lock);
    return 0;
}

int le_audio_stream_rx_drain(void *stream)
{
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)rx_stream->parent;

    /*r_printf("rx buffered : %d\n", cbuf_get_data_len(&rx_stream->buf.cbuf));*/
    spin_lock(&ctx->lock);
    cbuf_clear(&rx_stream->buf.cbuf);
    spin_unlock(&ctx->lock);
    return 0;
}

int le_audio_stream_rx_frame(void *stream, void *data, int len, u32 timestamp)
{
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)rx_stream->parent;
    struct le_audio_frame *frame = NULL;

    if (rx_stream->frames_len + len > rx_stream->frames_max_size) {
        /*printf("frame no buffer.\n");*/
        spin_lock(&ctx->lock);
        if (!list_empty(&rx_stream->frames)) {
            frame = list_first_entry(&rx_stream->frames, struct le_audio_frame, entry);
            list_del(&frame->entry);
            rx_stream->frames_len -= frame->len;
            free(frame);
        }
        spin_unlock(&ctx->lock);
        putchar('H');
        if (ctx->rx_tick_handler) {
            ctx->rx_tick_handler(ctx->rx_tick_priv);
        }
        return 0;
    }
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE //环绕音
    if (len > 2) { //丢包判断
        int rlen = 0;
#if (SURROUND_SOUND_FIX_ROLE_EN && (SURROUND_SOUND_ROLE == 1) || (SURROUND_SOUND_ROLE == 2))
        //如果角色固定，并且是立体声数据流
        //立体声(LS(解码左声道)//RS(解码右声道))
        rlen = get_enc_dual_output_frame_len(); //获取立体声编码帧长
        frame = malloc(sizeof(struct le_audio_frame) + rlen);
        if (!frame) {
            return 0;
        }
        frame->data = (u8 *)(frame + 1);
        memcpy(frame->data, data, rlen);
        frame->len = rlen;
        len = rlen;
#elif (SURROUND_SOUND_FIX_ROLE_EN && (SURROUND_SOUND_ROLE == 3))
        //如果角色固定，并且是单声道数据
        //单声道(SW)    62(立体声的编码长度)
        rlen = get_enc_mono_output_frame_len(); //获取单声道编码帧长
        int offset = get_enc_dual_output_frame_len(); // 获取立体声道编码帧长
        frame = malloc(sizeof(struct le_audio_frame) + rlen);
        if (!frame) {
            return 0;
        }
        frame->data = (u8 *)(frame + 1);
        memcpy(frame->data, (u8 *)data + offset, rlen);
        frame->len = rlen;
        len = rlen;
        /* printf("len :%d",frame->len); */
#elif (SURROUND_SOUND_FIX_ROLE_EN == 0)
        //如果角色不固定
        //立体声(LS(解码左声道)//RS(解码右声道))
        u8 role = get_surround_sound_role();
        if (role == SURROUND_SOUND_RX1_DUAL_L || role == SURROUND_SOUND_RX2_DUAL_R) {
            rlen = get_enc_dual_output_frame_len(); //获取立体声编码帧长
            frame = malloc(sizeof(struct le_audio_frame) + rlen);
            if (!frame) {
                return 0;
            }
            frame->data = (u8 *)(frame + 1);
            memcpy(frame->data, data, rlen);
            frame->len = rlen;
            len = rlen;
        } else if (role == SURROUND_SOUND_RX3_MONO) {
            //单声道(SW)    62(立体声的编码长度)
            rlen = get_enc_mono_output_frame_len(); //获取单声道编码帧长
            int offset = get_enc_dual_output_frame_len(); // 获取立体声道编码帧长
            frame = malloc(sizeof(struct le_audio_frame) + rlen);
            if (!frame) {
                return 0;
            }
            frame->data = (u8 *)(frame + 1);
            memcpy(frame->data, (u8 *)data + offset, rlen);
            frame->len = rlen;
            len = rlen;
        } else {
            ASSERT(0, "err!! %s, %d, surround sound role is error:%d\n", __func__, __LINE__, role);
        }

#endif
    } else { // 丢包数据
        frame = malloc(sizeof(struct le_audio_frame) + len);
        if (!frame) {
            return 0;
        }
        frame->data = (u8 *)(frame + 1);
        memcpy(frame->data, data, len);
        frame->len = len;
        put_buf(frame->data, len);
    }

    frame->timestamp = timestamp;
    rx_stream->timestamp = timestamp;


#else// 正常的广播接收
    frame = malloc(sizeof(struct le_audio_frame) + len);
    /* frame = malloc(sizeof(struct le_audio_frame) + len); */
    if (!frame) {
        return 0;
    }
    frame->len = len;
    frame->data = (u8 *)(frame + 1);
    frame->timestamp = timestamp;
    rx_stream->timestamp = timestamp;
    memcpy(frame->data, data, len);
#endif

    spin_lock(&ctx->lock);
    list_add_tail(&frame->entry, &rx_stream->frames);
    rx_stream->frames_len += len;
    if (ctx->rx_tick_handler) {
        ctx->rx_tick_handler(ctx->rx_tick_priv);
    }
    spin_unlock(&ctx->lock);
    return len;
}

static void le_audio_stream_rx_fill_frame(struct le_audio_rx_stream *rx_stream)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)rx_stream->parent;

    if (rx_stream->coding_type == AUDIO_CODING_LC3 || rx_stream->coding_type == AUDIO_CODING_JLA || rx_stream->coding_type == AUDIO_CODING_JLA_V2) {
        rx_stream->timestamp = (rx_stream->timestamp + ctx->fmt.isoIntervalUs) & 0xfffffff;
        u8 jla_err_frame[2] = {0x02, 0x00};
        u8 err_packet[10] = {0};
        u8 frame_num = ctx->fmt.isoIntervalUs / 100 / ctx->fmt.frame_dms;
        for (int i = 0; i < frame_num; i++) {
            memcpy(&err_packet[i * 2], jla_err_frame, 2);
        }
        le_audio_stream_rx_frame(rx_stream, err_packet, frame_num * 2, rx_stream->timestamp);
    }

}

void le_audio_stream_rx_disconnect(void *stream)
{
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)stream;
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)rx_stream->parent;

    spin_lock(&ctx->lock);
    rx_stream->online = 0;
    le_audio_stream_rx_fill_frame(rx_stream);

    rx_stream->fill_data_timer = sys_hi_timer_add(rx_stream, (void *)le_audio_stream_rx_fill_frame, ctx->fmt.isoIntervalUs / 1000);

    spin_unlock(&ctx->lock);
}

struct le_audio_frame *le_audio_stream_get_frame(void *le_audio)
{
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_rx_stream *rx_stream = (struct le_audio_rx_stream *)ctx->rx_stream;
    struct le_audio_frame *frame = NULL;

    if (!ctx || !rx_stream) {
        return NULL;
    }

    spin_lock(&ctx->lock);
    /* get_frame: */
    if (!list_empty(&rx_stream->frames)) {
        frame = list_first_entry(&rx_stream->frames, struct le_audio_frame, entry);
        list_del(&frame->entry);
    }

    //if (!frame && !rx_stream->online) {
    //  if (rx_stream->coding_type == AUDIO_CODING_LC3 || rx_stream->coding_type == AUDIO_CODING_JLA || rx_stream->coding_type == AUDIO_CODING_JLA_V2) {
    //   le_audio_stream_rx_fill_frame(rx_stream);
    //    goto get_frame;
    // }
    //}

    spin_unlock(&ctx->lock);

    return frame;
}

int le_audio_stream_get_frame_num(void *le_audio)
{
    if (!le_audio) {
        return 0;
    }
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_rx_stream *rx_stream = ctx->rx_stream;

    if (!rx_stream) {
        return 0;
    }

    return rx_stream->frames_len / rx_stream->isoIntervalUs_len;
}

void le_audio_stream_free_frame(void *le_audio, struct le_audio_frame *frame)
{
    if (!le_audio) {
        return;
    }
    struct le_audio_stream_context *ctx = (struct le_audio_stream_context *)le_audio;
    struct le_audio_rx_stream *rx_stream = ctx->rx_stream;
    if (!rx_stream) {
        return;
    }

    spin_lock(&ctx->lock);
    rx_stream->frames_len -= frame->len;
    spin_unlock(&ctx->lock);
    free(frame);
}

#endif /*LE_AUDIO_STREAM_ENABLE*/
