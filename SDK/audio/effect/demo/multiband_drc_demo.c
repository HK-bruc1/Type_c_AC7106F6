
#include "effects/effects_adj.h"
#include "effects/audio_multiband_drc.h"
#include "node_uuid.h"

void user_multiband_drc_update_parm_demo()
{
    struct cfg_info info = {0};
    char mode_index       = 0;               //模式序号（当前节点无多参数组，mode_index是0）
    char *node_name       = "mdrc_name";       //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    char cfg_index        = 0;               //目标配置项序号（当前节点无多参数组，cfg_index是0）
    int ret = jlstream_read_form_node_info_base(mode_index, node_name, cfg_index, &info);
    if (ret) {
        return;
    }
    u8 *data_buf = zalloc(info.size);
    if (!jlstream_read_form_cfg_data(&info, data_buf)) {
        printf("user mdrc cfg parm read err\n");
        free(data_buf);
        return;
    }
    struct mdrc_param_tool_set *mdrc_parm = zalloc(sizeof(struct mdrc_param_tool_set));
    multiband_drc_param_set(mdrc_parm, data_buf);
    free(data_buf);
    multiband_drc_param_debug(mdrc_parm);

    //common_parm 更新
    struct mdrc_common_param_update {
        int type;
        struct mdrc_common_param data;
    };
    struct mdrc_common_param_update *common_parm = zalloc(sizeof(struct mdrc_common_param_update));
    common_parm->type = COMMON_PARM;
    memcpy(&common_parm->data, &mdrc_parm->common_param, sizeof(struct mdrc_common_param));
    jlstream_set_node_param(NODE_UUID_MDRC, node_name, common_parm, sizeof(struct mdrc_common_param_update));


    //drc更新
    struct mdrc_drc_param_update {
        int type;
        int len;
        u8 data[0];
    };
    struct mdrc_drc_param_update *drc_parm = zalloc(wdrc_advance_get_param_size(64) + sizeof(int) * 2);
    u8 drc_parm_type[] = {LOW_DRC_PARM, MID_DRC_PARM, HIGH_DRC_PARM, WHOLE_DRC_PARM};
    for (u8 i = 0; i < 4; i++) {
        drc_parm->type = drc_parm_type[i];
        drc_parm->len = mdrc_parm->drc_len[i];
        memcpy(drc_parm->data, mdrc_parm->drc_param[i], drc_parm->len);
        jlstream_set_node_param(NODE_UUID_MDRC, node_name, drc_parm, drc_parm->len + sizeof(int) * 2);
    }


    multiband_drc_param_free(mdrc_parm);
    free(mdrc_parm);
    free(drc_parm);
    free(common_parm);
}
