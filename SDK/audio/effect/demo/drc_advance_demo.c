
#include "effects/effects_adj.h"
#include "effects/audio_wdrc_advance.h"
#include "node_uuid.h"

void user_drc_advance_update_parm_demo()
{
    struct cfg_info info = {0};
    char mode_index       = 0;               //模式序号（当前节点无多参数组，mode_index是0）
    char *node_name       = "drc_adv_name";       //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    char cfg_index        = 0;               //目标配置项序号（当前节点无多参数组，cfg_index是0）
    int ret = jlstream_read_form_node_info_base(mode_index, node_name, cfg_index, &info);
    if (ret) {
        return;
    }
    wdrc_advance_param_tool_set *wdrc_advance_parm = zalloc(info.size);
    if (!jlstream_read_form_cfg_data(&info, wdrc_advance_parm)) {
        printf("user wdrc_advance_parm cfg parm read err\n");
        free(wdrc_advance_parm);
        return;
    }
    jlstream_set_node_param(NODE_UUID_WDRC_ADVANCE, node_name, wdrc_advance_parm, wdrc_advance_get_param_size(wdrc_advance_parm->parm.threshold_num));
    free(wdrc_advance_parm);
}
