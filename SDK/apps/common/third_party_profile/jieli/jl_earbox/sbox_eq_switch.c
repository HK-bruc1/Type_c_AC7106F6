/* #include "asm/hw_eq.h" */
#include "app_config.h"
#include "node_uuid.h"
#include "effects/eq_config.h"
#include "sbox_core_config.h"
#include "sbox_user_app.h"
#include "sbox_eq_switch.h"

#define     VM_SBOX_EQ_INFO      46
#define     SBOX_EQ_SECTION_MAX       10

u8 user_custom_eq_index = 0;


struct eq_sbox_info {
    u8 index;
    //cppcheck-suppress unusedStructMember
    float eq_gain[SBOX_EQ_SECTION_MAX];
};

static struct eq_sbox_info local_eq_info = {0};

struct eq_seg_info custom_eq_tab[] = {
    {0, EQ_IIR_TYPE_BAND_PASS, 100,   0, 0.7f},
    {1, EQ_IIR_TYPE_BAND_PASS, 200,   0, 0.7f},
    {2, EQ_IIR_TYPE_BAND_PASS, 400,   0, 0.7f},
    {3, EQ_IIR_TYPE_BAND_PASS, 800,   0, 0.7f},
    {4, EQ_IIR_TYPE_BAND_PASS, 1000,  0, 0.7f},
    {5, EQ_IIR_TYPE_BAND_PASS, 3000,  0, 0.7f},
    {6, EQ_IIR_TYPE_BAND_PASS, 6000,  0, 0.7f},
    {7, EQ_IIR_TYPE_BAND_PASS, 10000, 0, 0.7f},
    {8, EQ_IIR_TYPE_BAND_PASS, 13000, 0, 0.7f},
    {9, EQ_IIR_TYPE_BAND_PASS, 16000, 0, 0.7f},
};

int user_eq_mode_set_custom_param(u16 index, int gain, int freq, int q_value, u16 filter)
{
    float q_temp = 0.0f;
    if (index >= EQ_SECTION_MAX) {
        return 0;
    }
    if (freq < 20) {
        freq = 16000;
        gain = 0;
        q_value = 7;
        filter = 0x02;
    }
    if (freq > 22000) {
        freq = 16000;
        gain = 0;
        q_value = 7;
        filter = 0x02;
    }
    if (gain > 12) {
        gain = 0;
    }
    if (gain < -12) {
        gain = 0;
    }
    if (q_value < 3) {
        q_value = 7;
    }
    if (q_value > 300) {
        q_value = 7;
    }
    if (filter > 0x04) {
        filter = EQ_IIR_TYPE_BAND_PASS;
    }
    if (filter < 0x00) {
        filter = EQ_IIR_TYPE_BAND_PASS;
    }

#if 0
    //printf("eq_tab_custom, freq:%d, gain:%d, q:%d, iir_type:%d\n",freq,gain,q_value,filter);
    eq_tab_custom[index].freq = freq;
    eq_tab_custom[index].gain = (float)gain;
    q_temp = (float)q_value;
    eq_tab_custom[index].q = (q_temp / 10);
    eq_tab_custom[index].iir_type = filter;
    r_printf("set eq_tab_custom, freq:%d, gain:%d.%d, q:%d.%d, iir_type:%d ", eq_tab_custom[index].freq,
             (int)eq_tab_custom[index].gain, user_abs((int)((int)(eq_tab_custom[index].gain * 100) % 100)),
             (int)eq_tab_custom[index].q,    user_abs((int)((int)(eq_tab_custom[index].q * 100) % 100)),
             eq_tab_custom[index].iir_type);
#else
    //printf("eq_tab_custom, freq:%d, gain:%d, q:%d, iir_type:%d\n",freq,gain,q_value,filter);
    custom_eq_tab[index].freq = freq;
    custom_eq_tab[index].gain = (float)gain;
    q_temp = (float)q_value;
    custom_eq_tab[index].q = (q_temp / 10);
    custom_eq_tab[index].iir_type = filter;
    // printf("custom, freq:%d, gain:%d.%d, q:%d.%d, iir_type:%d ",custom_eq_tab[index].freq,
    //         (int)custom_eq_tab[index].gain, user_abs((int)((int)(custom_eq_tab[index].gain*100)%100)),
    //         (int)custom_eq_tab[index].q,    user_abs((int)((int)(custom_eq_tab[index].q*100)%100)),
    //         custom_eq_tab[index].iir_type);
#endif

    return 0;
}


void custom_eq_param_update(void)
{
    /*
     *解析配置文件内效果配置
     * */
    struct cfg_info info  = {0};             //节点配置相关信息（参数存储的目标地址、配置项大小）
    char mode_index       = 0;               //模式序号（当前节点无多参数组，mode_index是0）
    char *node_name       = "MusicEqBt";     //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    char cfg_index        = 12;///0;         //目标配置项序号（当前节点无多参数组，cfg_index是0）
    int ret = jlstream_read_form_node_info_base(mode_index, node_name, cfg_index, &info);
    if (!ret) {
        //struct eq_tool *tab = zalloc(info.size);
        //if (!jlstream_read_form_cfg_data(&info, tab)) {
        //    printf("user eq cfg parm read err\n");
        //    free(tab);
        //    return;
        //}

        //运行时，直接设置更新
        struct eq_adj eff = {0};
        g_printf("base_custom_eq_param_update");
        eff.type = EQ_SEG_CMD;             //参数类型：滤波器参数
        eff.fade_parm.fade_time = 10;      //ms，淡入timer执行的周期
        eff.fade_parm.fade_step = 0.1f;    //淡入步进
        int nsection = ARRAY_SIZE(custom_eq_tab);//滤波器段数
        for (int j = 0; j < nsection; j++) {
            memcpy(&eff.param.seg, &custom_eq_tab[j], sizeof(struct eq_seg_info));//滤波器参数
            jlstream_set_node_param(NODE_UUID_EQ, node_name, &eff, sizeof(eff));
        }
    }
}


u8 sbox_get_eq_index(void)
{
    return user_custom_eq_index;
}

//切换工具内置的EQ参数表
// void sbox_control_eq_mode_switch(void *datas)
// {
//     struct eq_sbox_info *data = (struct eq_sbox_info *)datas;
//     user_custom_eq_index = data->index;

//     if(user_custom_eq_index == EQ_MODE_CUSTOM){

//         for(int i =0 ; i<SBOX_EQ_SECTION_MAX;i++){
//             custom_eq_tab[i].gain = data->eq_gain[i];
//         }
//         custom_eq_param_update();

//     }else{
//         printf("custom_control_eq_mode_switch %d\n",user_custom_eq_index);
//         eq_file_cfg_switch("MusicEqBt",user_custom_eq_index);
//     }
// }

void custom_control_eq_mode_switch(void *datas)
{
    u8 *data = (u8 *)datas;
    user_custom_eq_index = *data;
    printf("custom_control_eq_mode_switch %d\n", user_custom_eq_index);
    eq_file_cfg_switch("MusicEqBt", user_custom_eq_index);
}



void sbox_eq_init(void)
{
    u8 eq_tmp[11] = {0}; // 确保数组大小正确
    int ret = syscfg_read(VM_SBOX_EQ_INFO, eq_tmp, sizeof(eq_tmp));
    if (ret == sizeof(eq_tmp)) { // 检查返回值是否等于期望的长度
        printf("%s sbox_eq_init ok  ", __func__);
        printf("\n");
        put_buf(eq_tmp, ret); // 使用返回值作为参数
        custom_control_eq_mode_switch(eq_tmp);
    } else {
        printf("%s sbox_eq_init error %d\n", __func__, ret);

    }
}

void sbox_eq_reset(void)
{


}

