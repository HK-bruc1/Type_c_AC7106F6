#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".lhdc_x_demo.data.bss")
#pragma data_seg(".lhdc_x_demo.data")
#pragma const_seg(".lhdc_x_demo.text.const")
#pragma code_seg(".lhdc_x_demo.text")
#endif

#include "effects/effects_adj.h"
#include "framework/nodes/lhdc_x_node.h"
#include "node_uuid.h"


int user_lhdc_x_bypass_demo(char *node_name, u8 is_bypass)
{
    struct lhdc_x_param_tool_set cfg = {0};
    cfg.is_bypass = is_bypass;
    return jlstream_set_node_param(NODE_UUID_LHDC_X, node_name, &cfg, sizeof(cfg));
}
