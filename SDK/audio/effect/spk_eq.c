#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".spk_eq.data.bss")
#pragma data_seg(".spk_eq.data")
#pragma const_seg(".spk_eq.text.const")
#pragma code_seg(".spk_eq.text")
#endif

#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "audio_config.h"
#include "effects/effects_adj.h"
#include "effects/audio_spk_eq.h"
#include "jlstream.h"
#include "user_cfg_id.h"
#include "online_db_deal.h"

#if TCFG_SPEAKER_EQ_NODE_ENABLE



//如果以下声道数的配置覆盖不了所有情况的话，spk eq声道数需由用户根据实际声道数指定
#if TCFG_DAC_NODE_ENABLE

#if ((TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR) || (TCFG_AUDIO_DAC_CONNECT_MODE ==DAC_OUTPUT_DUAL_LR_DIFF))
static u8 spk_eq_ch_num = 2;
#else
static u8 spk_eq_ch_num = 1;
#endif

#else
static u8 spk_eq_ch_num = 2;
#endif

#define SPK_EQ_NSECTION  10
static u32 spk_eq_tab_size = 0;
static struct eq_seg_info *spk_eq_tab = NULL;//按实际声道数做申请,只申请,不做释放
static u8 spk_eq_read_from_ram = 0;
static float spk_eq_global_gain[2];
static char spk_eq_name[][16] = {"spk_eq", "SpeakerEQ1", "SpeakerEQ2", "SpeakerEQ3", "SpeakerEQ4", "SpeakerEQ5"}; //如果存在多个spk_eq节点,需在此添加名字

static void spk_eq_tab_init()
{
    if (!spk_eq_tab) {
        spk_eq_tab_size = sizeof(struct eq_seg_info) * SPK_EQ_NSECTION * spk_eq_ch_num;
        spk_eq_tab = malloc(spk_eq_tab_size);
        struct eq_seg_info seg;
        for (int i = 0; i < SPK_EQ_NSECTION; i++) {
            seg.index = i;
            seg.freq = 100;
            seg.iir_type = EQ_IIR_TYPE_BAND_PASS;
            seg.gain = 0.0f;
            seg.q = 0.7f;
            memcpy(&spk_eq_tab[i], &seg, sizeof(seg));
            if (spk_eq_ch_num == 2) {
                memcpy(&spk_eq_tab[i + SPK_EQ_NSECTION], &seg, sizeof(seg));
            }
        }
    }
}

/*
 *spk_eq 系数更新接口,更新第几段eq系数
 *parm: *seg
 *seg->index:第几段(0~9)
 *seg->iir_type:滤波器类型(EQ_IIR_TYPE)
 *seg->freq:中心截止频率(20~20kHz)
 *seg->gain:增益（-12~13dB）
 * */
void spk_eq_seg_update(struct eq_seg_info *seg)
{
    spk_eq_tab_init();
    //段系数更新
    struct spk_eq_seg_parm sparm;
    sparm.type  = UPDATE_SPK_EQ_SEG;
    sparm.left_right = 0;
    memcpy(&sparm.seg, seg, sizeof(struct eq_seg_info));
    int ret;
    for (int i = 0; i < ARRAY_SIZE(spk_eq_name); i++) {
        ret = jlstream_set_node_param(NODE_UUID_SPEAKER_EQ, spk_eq_name[i], &sparm, sizeof(sparm));
        if (ret != true) {
            continue;
        }
        break;
    }
    if (ret <= 0) {
        struct eq_seg_info *tar_seg = (struct eq_seg_info *)spk_eq_tab;
        memcpy(&tar_seg[sparm.seg.index], &sparm.seg, sizeof(struct eq_seg_info));
    }
}

/*
 *spk_eq 右声道系数更新接口,更新第几段eq系数
 *parm: *seg
 *seg->index:第几段(0~9)
 *seg->iir_type:滤波器类型(EQ_IIR_TYPE)
 *seg->freq:中心截止频率(20~20kHz)
 *seg->gain:增益（-12~13dB）
 * */
void spk_eq_seg_update_R(struct eq_seg_info *seg)
{
    if (spk_eq_ch_num != 2) {
        return;
    }

    spk_eq_tab_init();
    //段系数更新
    struct spk_eq_seg_parm sparm;
    sparm.type  = UPDATE_SPK_EQ_SEG;
    sparm.left_right = 1;
    memcpy(&sparm.seg, seg, sizeof(struct eq_seg_info));
    int ret;
    for (int i = 0; i < ARRAY_SIZE(spk_eq_name); i++) {
        ret = jlstream_set_node_param(NODE_UUID_SPEAKER_EQ, spk_eq_name[i], &sparm, sizeof(sparm));
        if (ret != true) {
            continue;
        }
        break;
    }
    if (ret <= 0) {
        struct eq_seg_info *tar_seg = (struct eq_seg_info *)&spk_eq_tab[SPK_EQ_NSECTION];
        memcpy(&tar_seg[sparm.seg.index], &sparm.seg, sizeof(struct eq_seg_info));
    }
}

/*
 *spk_eq 左声道总增益更新
 * */
void spk_eq_global_gain_udapte(float global_gain)
{
    struct spk_eq_global_gain gparm;
    gparm.type = UPDATE_SPK_EQ_GLOBAL_GAIN;
    gparm.left_right = 0;
    gparm.global_gain = global_gain;
    int ret;
    for (int i = 0; i < ARRAY_SIZE(spk_eq_name); i++) {
        ret = jlstream_set_node_param(NODE_UUID_SPEAKER_EQ, spk_eq_name[i], &gparm, sizeof(gparm));
        if (ret != true) {
            continue;
        }
        break;
    }
    spk_eq_global_gain[gparm.left_right] = gparm.global_gain;
}
/*
 *spk_eq 右声道总增益更新
 * */

void spk_eq_global_gain_udapte_R(float global_gain)
{
    struct spk_eq_global_gain gparm;
    gparm.type = UPDATE_SPK_EQ_GLOBAL_GAIN;
    gparm.left_right = 1;
    gparm.global_gain = global_gain;
    int ret;
    for (int i = 0; i < ARRAY_SIZE(spk_eq_name); i++) {
        ret = jlstream_set_node_param(NODE_UUID_SPEAKER_EQ, spk_eq_name[i], &gparm, sizeof(gparm));
        if (ret != true) {
            continue;
        }
        break;
    }
    spk_eq_global_gain[gparm.left_right] = gparm.global_gain;
}

int spk_eq_save_to_vm(void)
{
    spk_eq_tab_init();
    int ret_tmp = 0;
    struct spk_eq_get_seg_tab seg_tab = {0};
    seg_tab.type = GET_SPK_EQ_SEG_TAB;//获取系数表
    int ret;
    for (int i = 0; i < ARRAY_SIZE(spk_eq_name); i++) {
        ret = jlstream_get_node_param(NODE_UUID_SPEAKER_EQ, spk_eq_name[i], (void *)&seg_tab, sizeof(seg_tab));
        if (ret <= 0) {
            continue;
        }
        break;
    }
    if (ret <= 0) {
        seg_tab.seg = (struct eq_seg_info *)spk_eq_tab;
        seg_tab.tab_size = spk_eq_tab_size;
    }

    ret = syscfg_write(CFG_SPK_EQ_SEG_SAVE, seg_tab.seg, spk_eq_tab_size);
    if (ret <= 0) {
        printf("spk_eq tab write to vm err, ret %d\n", ret);
        ret_tmp = -1;
    }

    struct spk_eq_get_global_gain g_gain = {0};
    g_gain.type = GET_SPK_EQ_GLOBAL_GAIN;//获取总增益
    for (int i = 0; i < ARRAY_SIZE(spk_eq_name); i++) {
        ret = jlstream_get_node_param(NODE_UUID_SPEAKER_EQ, spk_eq_name[i], (void *)&g_gain, sizeof(g_gain));
        if (ret <= 0) {
            continue;
        }
        break;
    }
    if (ret <= 0) {
        memcpy(g_gain.global_gain, spk_eq_global_gain, sizeof(spk_eq_global_gain));
    }
    ret = syscfg_write(CFG_SPK_EQ_GLOBAL_GAIN_SAVE, g_gain.global_gain, sizeof(g_gain.global_gain));
    if (ret <= 0) {
        printf("spk_eq global gain write to vm err ret %d\n", ret);
        ret_tmp = -1;
    }
    return ret_tmp;
}
/*
 *spk_eq 系数表从vm中读取
 * */
int spk_eq_read_from_vm(void *priv)
{
    spk_eq_tab_init();
    struct spk_eq_get_parm *spk_parm = (struct spk_eq_get_parm *)priv;
    if (spk_parm->type == GET_SPK_EQ_SEG_TAB) {
        spk_parm->seg_tab.seg = (struct eq_seg_info *)spk_eq_tab;
        spk_parm->seg_tab.tab_size = spk_eq_tab_size / spk_eq_ch_num;
        //printf("-------spk tab %x, %d\n", (int)spk_eq_tab, spk_parm->seg_tab.tab_size);
    } else if (spk_parm->type == GET_SPK_EQ_VM_DATA) {
        struct spk_eq_get_seg_tab *seg_tab = (struct spk_eq_get_seg_tab *)&spk_parm->seg_tab;
        struct spk_eq_get_global_gain *g_gain = (struct spk_eq_get_global_gain *)&spk_parm->g_gain;

        if (spk_eq_read_from_ram) {
            memcpy(g_gain->global_gain, spk_eq_global_gain, sizeof(spk_eq_global_gain));
            return 0;
        }
        spk_eq_read_from_ram = 1;
        int ret = syscfg_read(CFG_SPK_EQ_SEG_SAVE, seg_tab->seg, spk_eq_tab_size);
        if (ret <= 0) {
            printf("skp_eq read from vm err\n");
            return -1;
        }
        ret = syscfg_read(CFG_SPK_EQ_GLOBAL_GAIN_SAVE, g_gain->global_gain, sizeof(g_gain->global_gain));
        if (ret <= 0) {
            printf("spk_eq global gain read from vm err\n");
            return -1;
        }
        memcpy(spk_eq_global_gain, g_gain->global_gain, sizeof(spk_eq_global_gain));
    }
    return 0;
}

typedef struct {
    u16 magic;     //0x3344
    u16 crc;       //data crc
    u8 data[32];   //data
} SPK_EQ_PACK;

#define CMD_SEG    0x1
#define CMD_GLOBAL 0x2
#define CMD_SAVE_PARM 0x3
#define CMD_RESET_PARM 0x4
/*
 *右声道更新命令
 * */
#define CMD_SEG_R        0x5
#define CMD_GLOBAL_R     0x6

#define CMD_SUPPORT_GLOBAL_GAIN     0x7


#define CMD_READ_SEG_L   0x8//左声道或者单声道获取系数表
#define CMD_READ_SEG_R   0x9//右声道获取系数表

#define CMD_READ_GLOBAL_L  0xa //左声道或者单声道获取总增益
#define CMD_READ_GLOBAL_R  0xb //右声道获取总增益
#define SPK_EQ_CRC_EN  0//是否使能crc校验
static u8 parse_seq = 0;

void (*send_data_handler)(u8 seq, u8 *packet, u8 size);

static int spk_eq_ack_packet(u8 seq, u8 *packet, u8 size)
{
    if (send_data_handler) {
        send_data_handler(seq, packet, size);
    } else {
        return app_online_db_ack(seq, packet, size);
    }
    return 0;
}
int spk_eq_spp_rx_packet(u8 *packet, u8 len)
{

    SPK_EQ_PACK pack = {0};//packet;
    memcpy(&pack, packet, len);
    if (pack.magic != 0x3344) {
        printf("magic err 0x%x\n", pack.magic);
        return -1;
    }
    struct eq_seg_info seg = {0};
    float global_gain = 0;
    u8 cmd = pack.data[0];
    u16 crc;
    printf("cmd %d\n", cmd);
    switch (cmd) {
    case CMD_SEG:
#if SPK_EQ_CRC_EN
        crc = CRC16(pack.data, len - 4);
        if (crc != pack.crc) {
            printf("spk_seg pack crc err %x, %x\n", crc, pack.crc);
            return -1;
        }
#endif
        memcpy(&seg, &pack.data[1], sizeof(struct eq_seg_info));
        spk_eq_seg_update(&seg);

        printf("idx:%d, iir:%d, frq:%d, gain:0x%x, q:0x%x \n", seg.index, seg.iir_type, seg.freq, *(int *)&seg.gain, *(int *)&seg.q);
        break;
    case CMD_GLOBAL:
#if SPK_EQ_CRC_EN
        crc = CRC16(pack.data, len - 4);
        if (crc != pack.crc) {
            printf("spk global gain info pack crc err %x, %x\n", crc, pack.crc);
            return -1;
        }
#endif
        memcpy(&global_gain, &pack.data[1], sizeof(float));
        spk_eq_global_gain_udapte(global_gain);
        printf("global_gain 0x%x\n", *(int *)&global_gain);
        break;
    case CMD_SEG_R:
#if SPK_EQ_CRC_EN
        crc = CRC16(pack.data, len - 4);
        if (crc != pack.crc) {
            printf("spk_seg_r pack crc err %x, %x\n", crc, pack.crc);
            return -1;
        }
#endif
        memcpy(&seg, &pack.data[1], sizeof(struct eq_seg_info));
        spk_eq_seg_update_R(&seg);

        printf("R idx:%d, iir:%d, frq:%d, gain:0x%x, q:0x%x \n", seg.index, seg.iir_type, seg.freq, *(int *)&seg.gain, *(int *)&seg.q);
        break;
    case CMD_GLOBAL_R:
#if SPK_EQ_CRC_EN
        crc = CRC16(pack.data, len - 4);
        if (crc != pack.crc) {
            printf("spk global gain info pack crc err %x, %x\n", crc, pack.crc);
            return -1;
        }
#endif
        memcpy(&global_gain, &pack.data[1], sizeof(float));
        spk_eq_global_gain_udapte_R(global_gain);
        printf("R global_gain 0x%x\n", *(int *)&global_gain);
        break;
    case  CMD_SAVE_PARM:
        return spk_eq_save_to_vm();
        break;
    case CMD_RESET_PARM:
        struct eq_seg_info seg;
        for (int i = 0; i < SPK_EQ_NSECTION; i++) {
            seg.index = i;
            seg.freq = 100;
            seg.iir_type = EQ_IIR_TYPE_BAND_PASS;
            seg.gain = 0.0f;
            seg.q = 0.7f;
            spk_eq_seg_update(&seg);
            spk_eq_seg_update_R(&seg);
        }
        spk_eq_global_gain_udapte(0);
        spk_eq_global_gain_udapte_R(0);
        return spk_eq_save_to_vm();
        break;
    case CMD_SUPPORT_GLOBAL_GAIN:
        //suuport global_gain
        break;
    case CMD_READ_SEG_L:
        spk_eq_ack_packet(parse_seq, (u8 *)spk_eq_tab, spk_eq_tab_size / spk_eq_ch_num);
        return 1;
    case CMD_READ_SEG_R:
        if (spk_eq_ch_num == 2) {
            spk_eq_ack_packet(parse_seq, (u8 *)&spk_eq_tab[SPK_EQ_NSECTION], spk_eq_tab_size / spk_eq_ch_num);
        }
        return 1;
    case CMD_READ_GLOBAL_L:
        spk_eq_ack_packet(parse_seq, (u8 *)&spk_eq_global_gain[0], 4);
        return 1;
    case CMD_READ_GLOBAL_R:
        spk_eq_ack_packet(parse_seq, (u8 *)&spk_eq_global_gain[1], 4);
        return 1;
    default:
        printf("crc %d\n", pack.crc);
        printf("cmd err %x\n", cmd);
        return -1;
    }
    return 0;
}

int spk_eq_read_seg_l(u8 **buf)
{
    *buf = (u8 *)(&spk_eq_tab[0]);
    return spk_eq_tab_size / spk_eq_ch_num;
}

int spk_eq_read_seg_r(u8 **buf)
{
    if (spk_eq_ch_num == 2) {
        *buf = (u8 *)(&spk_eq_tab[SPK_EQ_NSECTION]);
        return spk_eq_tab_size / spk_eq_ch_num;
    }
    return 0;
}

//读取SPK EQ总增益, ch: 0 L; 1 R
float spk_eq_read_global_gain(u8 ch)
{
    return spk_eq_global_gain[ch];
}

int spk_eq_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    if (ext_data) {
        parse_seq = ext_data[1];
    } else {
        parse_seq = 0xff;
    }
    printf("parse_seq %x\n", parse_seq);
    int ret = spk_eq_spp_rx_packet(packet, size);
    if (!ret) {
        u8 ack[] = "OK";
        spk_eq_ack_packet(parse_seq, ack, sizeof(ack));
    } else if (ret != 1) {
        u8 ack[] = "ER";
        spk_eq_ack_packet(parse_seq, ack, sizeof(ack));
    }
    return 0;
}

void spk_eq_set_send_data_handler(void (*handler)(u8 seq, u8 *packet, u8 size))
{
    send_data_handler = handler;
}

int spk_eq_init(void)
{
#if APP_ONLINE_DEBUG
    app_online_db_register_handle(DB_PKT_TYPE_SPK_EQ, spk_eq_app_online_parse);
#endif
    return 0;
}
__initcall(spk_eq_init);

//*********************************************************************************//
//                  在SDK开发阶段验收Speaker EQ是否有效,步骤如下
//1、 在可视化流程内拖入Speaker EQ节点。
//2、 代码打开宏TCFG_SPEAK_EQ_TEST_EN
//3、 编译下载，开机, 不做调节的情况下第一次测试一条频响曲线A。
//3、 按键发送消息APP_MSG_SPK_EQ_TEST（消息响应时会设置测试的默认曲线与总增益），第二次过ap测试频响曲线B。
//3、 关机断电，重新开机后，进行第三次过ap测试频响曲线C。频响曲线C与第二次的频响B曲线一致。
//4、 在第三次测试完成后，不关机（保持开机状态），进行第四次测试频响曲线D，频响曲线D与第二次的频响B曲线一致。
//5、 测试的频响曲线见表spk_eq_test_tab,测试总增益默认-2dB。
//6、 验收分析设置后的频响曲线B与C与D频响曲线重叠。B与C与D频响曲线最大值与频响曲线A重叠。
//7、 测试完毕关闭宏定义TCFG_SPEAK_EQ_TEST_EN
//*********************************************************************************//
#define TCFG_SPEAK_EQ_TEST_EN  0  //0:关闭  1：打开

#if defined(TCFG_SPEAK_EQ_TEST_EN) && TCFG_SPEAK_EQ_TEST_EN

struct eq_seg_info spk_eq_test_tab[] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 100,    2,  10},
    {1, EQ_IIR_TYPE_BAND_PASS, 500,    0,  10},
    {2, EQ_IIR_TYPE_BAND_PASS, 1000,   -2, 10},
    {3, EQ_IIR_TYPE_BAND_PASS, 1500,   0,  10},
    {4, EQ_IIR_TYPE_BAND_PASS, 2000,   2,  10},
    {5, EQ_IIR_TYPE_BAND_PASS, 4000,   0,  10},
    {6, EQ_IIR_TYPE_BAND_PASS, 6000,   -2, 10},
    {7, EQ_IIR_TYPE_BAND_PASS, 8000,   0,  10},
    {8, EQ_IIR_TYPE_BAND_PASS, 10000,  2,  10},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000,  -2, 10}
};

void spk_eq_test(void)
{
    printf("=====================spk_eq_test=================\n");
    if (!spk_eq_tab) {
        spk_eq_tab_size = sizeof(struct eq_seg_info) * SPK_EQ_NSECTION * spk_eq_ch_num;
        spk_eq_tab = malloc(spk_eq_tab_size);
    }
    if (spk_eq_tab && SPK_EQ_NSECTION <= ARRAY_SIZE(spk_eq_test_tab)) {
        memcpy(spk_eq_tab, spk_eq_test_tab, sizeof(struct eq_seg_info) * SPK_EQ_NSECTION);
        if (spk_eq_ch_num == 2) {
            memcpy(&spk_eq_tab[SPK_EQ_NSECTION], spk_eq_test_tab, sizeof(struct eq_seg_info) * SPK_EQ_NSECTION);
        }
        spk_eq_global_gain[0] = -2;//总增益-2dB
        spk_eq_global_gain[1] = -2;

        int ret = syscfg_write(CFG_SPK_EQ_SEG_SAVE, spk_eq_tab, spk_eq_tab_size);
        if (ret <= 0) {
            printf("spk_eq tab write to vm err, ret %d\n", ret);
        }
        ret = syscfg_write(CFG_SPK_EQ_GLOBAL_GAIN_SAVE, spk_eq_global_gain, sizeof(spk_eq_global_gain));
        if (ret <= 0) {
            printf("spk_eq global gain write to vm err ret %d\n", ret);
        }
    } else {
        printf("============spk_eq nsection error============\n");
    }
}
#else

void spk_eq_test(void)
{

}
#endif

#endif
