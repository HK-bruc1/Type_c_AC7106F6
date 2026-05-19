#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".tone_player.data.bss")
#pragma data_seg(".tone_player.data")
#pragma const_seg(".tone_player.text.const")
#pragma code_seg(".tone_player.text")
#endif
#include "tone_player.h"
#include "fs/resfile.h"
#include "os/os_api.h"
#include "system/init.h"
#include "decoder_node.h"
#include "jldemuxer.h"
#include "app_config.h"
#include "app_tone.h"
#include "audio_dac.h"

#if TCFG_TONE_NODE_ENABLE

extern const struct decoder_plug_ops decoder_plug_begin[];
extern const struct decoder_plug_ops decoder_plug_end[];
const struct stream_file_ops tone_file_ops;

static u8 g_player_id = 0;
static OS_MUTEX g_tone_mutex;
static struct list_head g_head = LIST_HEAD_INIT(g_head);
static struct list_head mtone_g_head = LIST_HEAD_INIT(mtone_g_head);
static u8 g_stream_index = 0;

static int tone_player_start(struct tone_player *);
static int tone_file_close(void *file);

__attribute__((weak))
void tone_event_to_user(int event, u16 fname_uuid)
{

}

static void *open_next_file(struct tone_player *player)
{
    if (player->next_file) {
        void *file = player->next_file;
        player->next_file = NULL;
        return file;
    }

    int index = ++player->index;

    do {
        if (player->file_name_list[index]) {
            char file_path[48];
            strcpy(file_path, FLASH_RES_PATH);
            strcpy(file_path + strlen(FLASH_RES_PATH), player->file_name_list[index]);
            void *file = resfile_open(file_path);
            if (file) {
                player->index = index;
                return file;
            }
            index++;
        } else {
            player->index = 0;
            break;
        }
    } while (index != player->index);

    return NULL;
}

void tone_player_free(struct tone_player *player)
{
    int fname_uuid = player->fname_uuid;
    void *cb_priv = player->priv;
    tone_player_cb_t callback = player->callback;

    if (--player->ref == 0) {
        tone_file_close(player);
        free(player);
    }
    if (callback) {
        callback(cb_priv,  STREAM_EVENT_STOP);
    } else {
        tone_event_to_user(STREAM_EVENT_STOP, fname_uuid);
    }
}

static void tone_player_callback(void *_player_id, int event)
{
    void *file = NULL;
    struct tone_player *player;

    printf("tone_callback: %x, %d\n", event, (u8)_player_id);

    switch (event) {
    case STREAM_EVENT_START:
        if (list_empty(&g_head)) {          //先判断是否为空防止触发异常
            break;
        }
        os_mutex_pend(&g_tone_mutex, 0);
        player = list_first_entry(&g_head, struct tone_player, entry);
        if (player->player_id == (u8)_player_id) {
            if (player->callback) {
                player->callback(player->priv,  STREAM_EVENT_START);
            }
        }
        os_mutex_post(&g_tone_mutex);
        break;
    case STREAM_EVENT_PREEMPTED:
    case STREAM_EVENT_NEGOTIATE_FAILD:
    /*
     * 提示音被抢占或者参数协商失败,直接结束播放
     */
    case STREAM_EVENT_STOP:
        if (list_empty(&g_head)) {          //先判断是否为空防止触发异常
            break;
        }
        os_mutex_pend(&g_tone_mutex, 0);
        player = list_first_entry(&g_head, struct tone_player, entry);
        if (player->player_id != (u8)_player_id) {
            os_mutex_post(&g_tone_mutex);
            printf("player_id_not_match: %d\n", player->player_id);
            break;
        }

        if (event == STREAM_EVENT_STOP) {
            /*
             * 打开文件列表的下一个文件,重新启动解码
             */
            while (1) {
                file = open_next_file(player);
                if (!file) {
                    break;
                }
                resfile_close(player->file);
                player->file = file;
                int err = jlstream_start(player->stream);
                if (err == 0) {
                    break;
                }
            }
        }

        if (!file) {
            jlstream_release(player->stream);
            DAC_NOISEGATE_ON();

            list_del(&player->entry);
__try_play_next_tone:
            if (!list_empty(&g_head)) {
                struct tone_player *p;
                p = list_first_entry(&g_head, struct tone_player, entry);
                int ret = tone_player_start(p);
                if (ret) { //提示音播放失败，继续查询下一个
                    puts("__try_play_next_tone\n");
                    goto __try_play_next_tone;
                }
            }

            tone_player_free(player);
        }

        os_mutex_post(&g_tone_mutex);
        break;
    }
}

static int tone_file_read(void *p_file, u8 *buf, int len)
{
    int offset = 0;
    struct tone_player *player = (struct tone_player *)p_file;

    while (len) {
        if (!player->file) {
            break;
        }
        int rlen = resfile_read(player->file, buf + offset, len);
        if (rlen < 0) {
            break;
        }
        offset += rlen;
        if ((len -= rlen) == 0) {
            break;
        }
        if (player->scene == STREAM_SCENE_RING && player->coding_type != AUDIO_CODING_MTY) {
            if (player->coding_type == AUDIO_CODING_WTGV2) {
                //WTS拼接提示音时，去掉头1byte
                resfile_seek(player->file, 1, RESFILE_SEEK_SET);
            } else {
                resfile_seek(player->file, 0, RESFILE_SEEK_SET);
                if (player->coding_type == AUDIO_CODING_MP3) {
                    break;
                }
            }
            continue;
        }
        if (player->next_file) {
            break;
        }

        /*
         * 打开文件列表中的下一个文件,检查format是否和当前文件相同
         * 如果相同就继读下一个文件, 可以保证TWS同步播放文件列表情况下的音频同步
         */
        int index = player->index;
        void *file = open_next_file(player);
        if (!file) {
            player->index = index;
            break;
        }
        char name[16];
        resfile_get_name(file, name, 16);
        struct stream_file_ops file_ops = {
            .read       = (int (*)(void *, u8 *, int))resfile_read,
            .seek       = (int (*)(void *, int, int))resfile_seek,
        };
        struct stream_file_info file_info = {
            .file   = file,
            .fname  = name,
            .ops    = &file_ops,
        };
        struct stream_fmt fmt;
        int err = jldemuxer_get_tone_file_fmt(&file_info, &fmt);
        if (err == 0) {
            if (player->coding_type == fmt.coding_type &&
                player->sample_rate == fmt.sample_rate &&
                player->channel_mode == fmt.channel_mode &&
                (player->scene != STREAM_SCENE_TONE || player->coding_type != AUDIO_CODING_MTY) && //mty格式提示音不能在原先的基础上接着读,同一个文件的铃声可以
                player->coding_type != AUDIO_CODING_F2A &&
                player->coding_type != AUDIO_CODING_MP3
               ) {
                resfile_close(player->file);
                player->file = file;
                if (player->coding_type == AUDIO_CODING_WTGV2) {
                    //WTS拼接提示音时，去掉头1byte
                    resfile_seek(player->file, 1, RESFILE_SEEK_SET);
                }
                continue;
            }
        }
        player->next_file = file;
        break;
    }

    return offset;
}

static int tone_file_seek(void *file, int offset, int fromwhere)
{
    struct tone_player *player = (struct tone_player *)file;

    return resfile_seek(player->file, offset, fromwhere);
}

static int tone_file_flen(void *file)
{
    struct tone_player *player = (struct tone_player *)file;
    u32 len = 0;
    if (player->file) {
        len = resfile_get_len(player->file);
    }
    return len;
}

static int tone_file_close(void *file)
{
    struct tone_player *player = (struct tone_player *)file;

    if (player->file) {
        resfile_close(player->file);
        player->file = NULL;
    }
    if (player->next_file) {
        resfile_close(player->next_file);
        player->next_file = NULL;
    }
    return 0;
}

static int tone_file_get_fmt(void *file, struct stream_fmt *fmt)
{
    struct tone_player *player = (struct tone_player *)file;

    char name[16];
    resfile_get_name(player->file, name, 16);

    struct stream_file_info file_info = {
        .file   = player,
        .fname  = name,
        .ops    = &tone_file_ops,
    };
    int err = jldemuxer_get_tone_file_fmt(&file_info, fmt);
    if (err) {
        player->coding_type = AUDIO_CODING_UNKNOW;
        return err;
    }

    player->coding_type = fmt->coding_type;
    player->sample_rate = fmt->sample_rate;
    player->channel_mode = fmt->channel_mode;

    return 0;
}

const struct stream_file_ops tone_file_ops = {
    .read       = tone_file_read,
    .seek       = tone_file_seek,
    .close      = tone_file_close,
    .get_fmt    = tone_file_get_fmt,
};

int tone_player_get_cur_time(struct tone_player *player)
{
    os_mutex_pend(&g_tone_mutex, 0);

    if (list_empty(&(g_head))) {          //先判断是否为空
        os_mutex_post(&g_tone_mutex);
        return -1;
    }
    if (!player) {
        player = list_first_entry(&g_head, struct tone_player, entry);
    }
    if (player && player->stream) {
        int ret = jlstream_node_ioctl(player->stream, NODE_UUID_DECODER, NODE_IOC_GET_CUR_TIME, 0);
        os_mutex_post(&g_tone_mutex);
        return ret;
    }
    os_mutex_post(&g_tone_mutex);

    return -1;
}

int tone_player_get_total_time(struct tone_player *player)
{
    os_mutex_pend(&g_tone_mutex, 0);

    if (list_empty(&(g_head))) {          //先判断是否为空
        os_mutex_post(&g_tone_mutex);
        return -1;
    }
    if (!player) {
        player = list_first_entry(&g_head, struct tone_player, entry);
    }
    if (player && player->stream) {
        int ret = jlstream_node_ioctl(player->stream, NODE_UUID_DECODER, NODE_IOC_GET_TOTAL_TIME, 0);
        os_mutex_post(&g_tone_mutex);
        return ret;
    }
    os_mutex_post(&g_tone_mutex);

    return -1;
}

static int tone_player_start(struct tone_player *player)
{
    int err = -EINVAL;

    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"tone");

    player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_TONE);
    if (!player->stream) {
        goto __exit0;
    }
    int player_id = player->player_id;
    jlstream_set_callback(player->stream, (void *)player_id, tone_player_callback);
    jlstream_set_scene(player->stream, player->scene);
    jlstream_set_coexist(player->stream, player->coexist);
    jlstream_node_ioctl(player->stream, NODE_UUID_DECODER,
                        NODE_IOC_SET_FILE_LEN, (int)tone_file_flen(player));

    if (player->callback) {
        err = player->callback(player->priv, STREAM_EVENT_INIT);
        if (err) {
            goto __exit1;
        }
    }
    jlstream_set_dec_file(player->stream, player, &tone_file_ops);

    DAC_NOISEGATE_OFF();
    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }

    return 0;

__exit1:
    jlstream_release(player->stream);
    DAC_NOISEGATE_ON();
__exit0:
    list_del(&player->entry);
    tone_player_free(player);
    return err;
}


int tone_player_init(struct tone_player *player, const char *file_name)
{
    void *file = NULL;
    int fname_uuid = 0;

    player->ref = 1;
    if (file_name) {
        char file_path[64];
        strcpy(file_path, FLASH_RES_PATH);
        strcpy(file_path + strlen(FLASH_RES_PATH), file_name);
        file = resfile_open(file_path);
        if (!file) {
            printf("tone_player open file faild: %s\n", file_name);
            return -EINVAL;
        }
        fname_uuid = JBHash((u8 *)file_name, strlen(file_name));
        printf("tone_player: %s\n", file_name);
    }
    player->file        = file;
    player->scene       = STREAM_SCENE_TONE;
    player->player_id   = g_player_id++;
    player->fname_uuid  = fname_uuid;
    player->coexist     = STREAM_COEXIST_AUTO;
    INIT_LIST_HEAD(&player->entry);

    return 0;
}

static struct tone_player *tone_player_create(const char *file_name)
{
    struct tone_player *player;

    player = zalloc(sizeof(*player));
    if (!player) {
        return NULL;
    }

    int err = tone_player_init(player, file_name);
    if (err) {
        tone_player_free(player);
        return NULL;
    }

    return player;
}

int tone_player_add(struct tone_player *player)
{
    os_mutex_pend(&g_tone_mutex, 0);

    if (list_empty(&g_head)) {
        int err = tone_player_start(player);
        if (err) {
            os_mutex_post(&g_tone_mutex);
            return err;
        }
    }
    list_add_tail(&player->entry, &g_head);

    os_mutex_post(&g_tone_mutex);
    return 0;
}

int play_tone_file(const char *file_name)
{
    struct tone_player *player;
#ifdef  CONFIG_FPGA_ENABLE
    /* return -ENOMEM; */
#endif

    player = tone_player_create(file_name);
    if (!player) {
        return -ENOMEM;
    }
    return tone_player_add(player);
}

int play_tone_file_callback(const char *file_name, void *priv, tone_player_cb_t callback)
{
    struct tone_player *player;
#ifdef  CONFIG_FPGA_ENABLE
    /* return -ENOMEM; */
#endif

    player = tone_player_create(file_name);
    if (!player) {
        callback(priv, STREAM_EVENT_STOP);
        return -ENOMEM;
    }
    player->priv        = priv;
    player->callback    = callback;

    return tone_player_add(player);
}

static struct tone_player *tone_files_create(const char *const file_name[], u8 file_num)
{
    struct tone_player *player;

    if (file_num > MAX_FILE_NUM) {
        return NULL;
    }

    player = tone_player_create(file_name[0]);
    if (!player) {
        return NULL;
    }
    for (int i = 0; i < file_num; i++) {
        player->file_name_list[i] = file_name[i];
    }
    return player;
}

int play_tone_files(const char *const file_name[], u8 file_num)
{
    struct tone_player *player;

    player = tone_files_create(file_name, file_num);
    if (!player) {
        return -EFAULT;
    }
    return tone_player_add(player);
}

int play_tone_files_alone(const char *const file_name[], u8 file_num)
{
    struct tone_player *player;

    player = tone_files_create(file_name, file_num);
    if (!player) {
        return -EFAULT;
    }
    player->coexist = STREAM_COEXIST_DISABLE;
    return tone_player_add(player);
}

int play_tone_files_callback(const char *const file_name[], u8 file_num,
                             void *priv, tone_player_cb_t callback)
{

    struct tone_player *player;

    player = tone_files_create(file_name, file_num);
    if (!player) {
        callback(priv, STREAM_EVENT_STOP);
        return -EFAULT;
    }
    player->priv = priv;
    player->callback = callback;
    return tone_player_add(player);
}

int play_tone_files_alone_callback(const char *const file_name[], u8 file_num,
                                   void *priv, tone_player_cb_t callback)
{

    struct tone_player *player;

    player = tone_files_create(file_name, file_num);
    if (!player) {
        callback(priv, STREAM_EVENT_STOP);
        return -EFAULT;
    }
    player->priv = priv;
    player->callback = callback;
    player->coexist = STREAM_COEXIST_DISABLE;
    return tone_player_add(player);
}

int play_tone_file_alone(const char *file_name)
{
    struct tone_player *player;

    player = tone_player_create(file_name);
    if (!player) {
        return -ENOMEM;
    }
    player->coexist = STREAM_COEXIST_DISABLE;

    return tone_player_add(player);
}


int play_tone_file_alone_callback(const char *file_name, void *priv,
                                  tone_player_cb_t callback)
{
    struct tone_player *player;

    player = tone_player_create(file_name);
    if (!player) {
        callback(priv, STREAM_EVENT_STOP);
        return -ENOMEM;
    }
    player->coexist     = STREAM_COEXIST_DISABLE;
    player->priv        = priv;
    player->callback    = callback;

    return tone_player_add(player);
}

int tone_player_runing()
{
    local_irq_disable();
    int ret = list_empty(&g_head) ? 0 : 1;
    local_irq_enable();
    return ret;
}

void tone_player_stop()
{
    struct tone_player *player, *n;

    os_mutex_pend(&g_tone_mutex, 0);

    list_for_each_entry_safe(player, n, &g_head, entry) {
        __list_del_entry(&player->entry);
        if (player->stream) {
            jlstream_stop(player->stream, 50);
            jlstream_release(player->stream);
            DAC_NOISEGATE_ON();
        }
        tone_player_free(player);
    }

    os_mutex_post(&g_tone_mutex);
}

u16 tone_player_get_fname_uuid(const char *fname)
{
    return JBHash((u8 *)fname, strlen(fname));
}


__INITCALL_BANK_CODE
static int __tone_player_init()
{
    os_mutex_create(&g_tone_mutex);
    return 0;
}
__initcall(__tone_player_init);

#if FILE_DEC_REPEAT_EN
static	struct audio_repeat_mode_param rep = {0};

static int common_dec_repeat_cb(void *priv)
{
    y_printf("common_dec_repeat_cb\n");
    return 0;
}

int common_dec_repeat_set(struct jlstream *stream, struct fixphase_repair_obj *repair_buf) //设置对应数据流无缝循环播放
{
    rep.flag = 1; //使能
    rep.headcut_frame = 2; //依据需求砍掉前面几帧，仅mp3格式有效
    rep.tailcut_frame = 2; //依据需求砍掉后面几帧，仅mp3格式有效
    rep.repeat_callback = common_dec_repeat_cb;
    rep.repair_buf = repair_buf;
    jlstream_node_ioctl(stream, NODE_UUID_DECODER, NODE_IOC_DECODER_REPEAT, (int)&rep);
    return 0;
}
#endif

static void fileplay_player_callback(void *_player_id, int event)
{
    void *file = NULL;
    struct tone_player *player = (struct tone_player *)_player_id;

    printf("fileplay_callback: %x, %p\n", event, _player_id);

    switch (event) {
    case STREAM_EVENT_START:
        break;
    case STREAM_EVENT_PREEMPTED:
    case STREAM_EVENT_NEGOTIATE_FAILD:
    /*
     * 提示音被抢占或者参数协商失败,直接结束播放
     */
    case STREAM_EVENT_STOP:
        if (!player) {
            break;
        }
        if (list_empty(&mtone_g_head)) {          //先判断是否为空防止触发异常
            break;
        }
        os_mutex_pend(&g_tone_mutex, 0);
        jlstream_release(player->stream);
        list_del(&player->entry);
        tone_file_close(player);
        g_stream_index &= ~BIT(player->stream_index);
        free(player);
        os_mutex_post(&g_tone_mutex);
        break;
    }
}

static int fileplay_player_start(struct tone_player *player, u8 index)
{
    int err = -EINVAL;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"tone");
    char stream_source_name[12] = "FilePlay";
    sprintf(stream_source_name, "%s%d", stream_source_name, index);
    /* printf("FILE PLAY :%d,%s\n",__LINE__,stream_source_name); */
    player->stream = jlstream_pipeline_parse_by_node_name(uuid, stream_source_name);
    if (!player->stream) {
        goto __exit0;
    }
    int player_id = player->player_id;
    jlstream_set_callback(player->stream, (void *)player, fileplay_player_callback);
    jlstream_set_scene(player->stream, player->scene);
    jlstream_set_coexist(player->stream, player->coexist);
    jlstream_node_ioctl(player->stream, NODE_UUID_DECODER,
                        NODE_IOC_SET_FILE_LEN, (int)tone_file_flen(player));
    if (player->callback) {
        err = player->callback(player->priv, STREAM_EVENT_INIT);
        if (err) {
            goto __exit1;
        }
    }
    jlstream_set_dec_file(player->stream, player, &tone_file_ops);
    err = jlstream_start(player->stream);
#if FILE_DEC_REPEAT_EN
    common_dec_repeat_set(player->stream, &player->repair_buf);
#endif
    if (err) {
        goto __exit1;
    }
    list_add_tail(&player->entry, &mtone_g_head);
    return 0;
__exit1:
    jlstream_release(player->stream);
__exit0:
    list_del(&player->entry);
    tone_player_free(player);
    return err;
}

struct tone_player *play_fileplay_file(const char *file_name, u8 index)
{
    struct tone_player *player;
#ifdef  CONFIG_FPGA_ENABLE
    /* return -ENOMEM; */
#endif

    os_mutex_pend(&g_tone_mutex, 0);
    if (g_stream_index & BIT(index)) {
        os_mutex_post(&g_tone_mutex);
        return NULL;
    }

    player = tone_player_create(file_name);
    if (!player) {
        os_mutex_post(&g_tone_mutex);
        return NULL;
    }
    g_stream_index |= BIT(index);
    player->stream_index = index;
    int err = fileplay_player_start(player, index);
    printf("play_fileplay_file %d\n", err);
    os_mutex_post(&g_tone_mutex);
    if (!err) {
        return player;
    } else {
        return NULL;
    }
}

#if TCFG_DEC_ENGINE_SOUND_ENABLE
#include "codec/carEngine_synth_h.h"
static void engine_player_callback(void *private_data, int event)
{
    void *file = NULL;
    struct tone_player *player;
    struct jlstream *stream = (struct jlstream *)private_data;
    switch (event) {
    case STREAM_EVENT_START:
        break;
    }
}

#define ENGINE_FILE_PATH  FLASH_RES_PATH"out.sound"
void engine_file_close(struct tone_player *player)
{
    if (player) {
        if (player->file) {
            resfile_close(player->file);
            player->file = NULL;
        }
        free(player);
    }
}

struct tone_player *engine_file_play(void *path)
{
    struct tone_player *player = zalloc(sizeof(struct tone_player));
    player->file = resfile_open((const char *)path);
    if (!player->file) {
        printf("engine_player_faild: %s\n", path);
        return NULL;
    }
    //需要新建数据流程图,不可直接用提示音的数据流程图,不然播提示音时相当于同一条数据流启动了两次
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"music");
    player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_TONE);
    if (!player->stream) {
        goto __exit0;
    }
    jlstream_set_callback(player->stream, player->stream, engine_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_TONE);
    jlstream_set_coexist(player->stream, 0);
    jlstream_set_dec_file(player->stream, player, &tone_file_ops);
    int err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }
    return player;
__exit1:
    jlstream_release(player->stream);
__exit0:
    engine_file_close(player);
    return NULL;

}

//摩托引擎声解码控制
//cmd:命令，可选：FIRE_ON,FIRE_OFF,SET_LEVEL,SET_MAX_LEVEL,SET_LEVEL_SPEEDUP
//arg:命令的参数值：

int engine_ioctl(struct jlstream *stream, enum engine_cmd cmd, int arg)
{
    struct engine_parm parm = {
        cmd,
        arg,
    };
    int ret = -1;
    if (stream) {
        ret = jlstream_node_ioctl(stream, NODE_UUID_DECODER, NODE_IOC_SET_PRIV_FMT, (int)&parm);
    }
    return ret;
}

void engine_sound_play_demo(void)
{
    struct tone_player *player = engine_file_play(ENGINE_FILE_PATH);
    if (player && player->stream) {
        engine_ioctl(player->stream, FIRE_ON, 0);
        engine_ioctl(player->stream, SET_LEVEL, 40); //最高到40
    }
}
#endif

int fileplay_runing()
{
    local_irq_disable();
    int ret = list_empty(&mtone_g_head) ? 0 : 1;
    local_irq_enable();
    return ret;
}


//指定需要关闭的文件播放器
void fileplay_close(struct tone_player *player)
{
    os_mutex_pend(&g_tone_mutex, 0);
    struct tone_player *_player, *n;
    if (player) {
        list_for_each_entry_safe(_player, n, &mtone_g_head, entry) {
            if (_player == player) {
                __list_del_entry(&_player->entry);
                if (_player->stream) {
                    jlstream_stop(_player->stream, 50);
                    jlstream_release(_player->stream);
                }
                tone_file_close(_player);
                g_stream_index &= ~BIT(_player->stream_index);
                free(_player);
                break;
            }
        }
    }
    os_mutex_post(&g_tone_mutex);
}
//关闭所有文件播放器
void fileplay_close_all()
{
    os_mutex_pend(&g_tone_mutex, 0);
    struct tone_player *player, *n;
    list_for_each_entry_safe(player, n, &mtone_g_head, entry) {
        __list_del_entry(&player->entry);
        if (player->stream) {
            jlstream_stop(player->stream, 50);
            jlstream_release(player->stream);
        }
        tone_file_close(player);
        g_stream_index &= ~BIT(player->stream_index);
        free(player);
    }
    os_mutex_post(&g_tone_mutex);

}

void multifile_play_demo(void)
{
    play_fileplay_file(get_tone_files()->bt_disconnect, 0);
    play_fileplay_file(get_tone_files()->bt_disconnect, 1);
    play_fileplay_file(get_tone_files()->bt_disconnect, 2);
    play_fileplay_file(get_tone_files()->bt_disconnect, 3);
    play_fileplay_file(get_tone_files()->bt_disconnect, 4);
    play_fileplay_file(get_tone_files()->bt_disconnect, 5);
    play_fileplay_file(get_tone_files()->bt_disconnect, 6);
    play_fileplay_file(get_tone_files()->bt_disconnect, 7);
}






#else
const struct stream_file_ops tone_file_ops;
int play_tone_file_callback(const char *file_name, void *priv, tone_player_cb_t callback)
{
    return 0;
}
int play_tone_file(const char *file_name)
{
    return 0;
}
void tone_player_stop()
{
}

int tone_player_runing()
{
    return 0;
}
int play_tone_file_alone(const char *file_name)
{
    return 0;
}

void tone_player_free(struct tone_player *player)
{
}


int tone_player_init(struct tone_player *player, const char *file_name)
{
    return 0;
}
#endif

