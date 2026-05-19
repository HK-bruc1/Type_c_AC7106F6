#ifndef  _CARENGGINE_SYNTH_H_
#define  _CARENGGINE_SYNTH_H_

#include "generic/typedef.h"
#include "if_decoder_ctrl.h"

#ifndef u32
#define u32 unsigned int
#endif


typedef struct _carEngine_synframe_context {
    u32(*needbuf)();
    u32(*open)(void *work_buf, const struct if_decoder_io *decoder_io);
    u32(*run)(void *work_buf);
    u32(*get_sr)(void *work_buf);
    u32(*setLevel)(void *work_buf, int level);
    u32(*FireON)(void *work_buf);
    u32(*set_maxlevel)(void *work_buf, int maxlevel);
    void (*set_level_speedup)(void *work_buf, int levelspeedup);
    u32(*FireOff)(void *work_buf);
} carEngine_synframe_context;


extern carEngine_synframe_context *get_carE_obj();

enum engine_cmd {
    FIRE_ON = 1,
    FIRE_OFF,
    SET_LEVEL,
    SET_MAX_LEVEL,
    SET_LEVEL_SPEEDUP,
};

struct engine_parm {
    enum engine_cmd cmd;
    int arg;
};


#endif
