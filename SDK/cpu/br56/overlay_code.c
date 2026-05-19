#include "cpu/includes.h"
#include "media/includes.h"
#include "overlay_code.h"


struct code_type {
    u32 type;
    u32 dst;
    u32 src;
    u32 size;
    u32 index;
};

extern int aec_addr, aec_begin, aec_size;
extern int aac_addr, aac_begin, aac_size;


const struct code_type  ctype[] = {
    {OVERLAY_AEC, (u32) &aec_addr, (u32) &aec_begin, (u32) &aec_size, 2},
    {OVERLAY_M4A, (u32) &aac_addr, (u32) &aac_begin, (u32) &aac_size, 3},
};

struct audio_overlay_type {
    u32 atype;
    u32 otype;
};

const struct audio_overlay_type  aotype[] = {

    {AUDIO_CODING_MSBC, OVERLAY_AEC },
    {AUDIO_CODING_CVSD, OVERLAY_AEC },
    {AUDIO_CODING_AAC,  OVERLAY_M4A },
};

void overlay_load_code(u32 type)
{
    static u32 type_flag = OVERLAY_RES;
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(ctype); i++) {
        if (type == ctype[i].type) {
            if (type != type_flag) {
                type_flag = type;
                if (ctype[i].dst != 0) {
                    load_overlay_code(ctype[i].index);
                    //memcpy((void *)ctype[i].dst, (void *)ctype[i].src, (int)ctype[i].size);
                }
            } else {
                puts("overlay: the same type, do NOT load again");
            }
            break;
        }
    }
}

void audio_overlay_load_code(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(aotype); i++) {
        if (type == aotype[i].atype) {
            overlay_load_code(aotype[i].otype);
        }
    }
}
