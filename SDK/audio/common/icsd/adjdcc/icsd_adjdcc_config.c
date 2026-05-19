#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"

#if ( TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN  && \
	TCFG_AUDIO_ADAPTIVE_DCC_ENABLE )

#include "audio_anc_includes.h"

#include "icsd_adjdcc.h"
#include "icsd_adt.h"

int (*dcc_printf)(const char *format, ...) = _dcc_printf;

void anc_core_ff_adjdcc_par_set(u8 dc_par);

const float err_overload_list[] = {90, 90, 85, 80};

const float adjdcc_param_table[] = {
    0, 100, 100,
    0,   0,  -10,
    0,   0,  -3,
    0,   0,  -5,
    0,   0,  -5,
    0,   0,  -5,
    0,   0,  -5,
    0,   0,  -5,
};

const float adjdcc_thr_list_up[] = {
    95, 105, 110, 120, 130, 140, 150, 160
};

const float adjdcc_thr_list_down[] = {
    80, 75, 95, 100, 90, 100, 110, 120,
};

const float adjdcc_iir_coef = 0.0;
const u8 adjdcc_steps = 2;

__adjdcc_config *_adjdcc_config = NULL;

__adjdcc_config *adjdcc_config_init()
{

    if (_adjdcc_config == NULL) {
        _adjdcc_config = anc_malloc("ICSD_ADJDCC", sizeof(__adjdcc_config));
    }

#if 0 //TCFG_AUDIO_ANC_EXT_TOOL_ENABLE  TODO
#endif
    {
        _adjdcc_config->release_cnt = 20;
        _adjdcc_config->steps = adjdcc_steps; // max:10

        for (int i = 0; i < 3 * 8; i++) {
            _adjdcc_config->param_table[i] = adjdcc_param_table[i];
        }

        for (int i = 0; i < 8; i++) {
            _adjdcc_config->thr_list_up[i] = adjdcc_thr_list_up[i];
            _adjdcc_config->thr_list_down[i] = adjdcc_thr_list_down[i];
        }

        _adjdcc_config->iir_coef = adjdcc_iir_coef;
        _adjdcc_config->wind_lvl_thr = 30;
    }

#if 0
    printf("adjdcc confit %d, %d, %d\n", _adjdcc_config->release_cnt, _adjdcc_config->wind_lvl_thr, _adjdcc_config->steps);
    printf("iir_coef : %d\n", (int)(_adjdcc_config->iir_coef * 100));

    for (int i = 0; i < 8; i++) {
        printf("thrh:%d, thrl:%d\n", (int)_adjdcc_config->thr_list_up[i], (int)_adjdcc_config->thr_list_down[i]);
    }

    for (int i = 0; i < 3 * 8; i++) {
        printf("%d\n", (int)(_adjdcc_config->param_table[i]));
    }
#endif
    return _adjdcc_config;
}

const struct adjdcc_function ADJDCC_FUNC_t = {
    .ff_adjdcc_par_set = anc_core_ff_adjdcc_par_set,
    .adjdcc_trigger_update = icsd_adt_alg_adjdcc_trigger_update,
    .rtanc_adjdcc_flag_set = icsd_adt_alg_rtanc_adjdcc_flag_set,
    .rtanc_adjdcc_flag_get = icsd_adt_alg_rtanc_adjdcc_flag_get,
    .get_wind_lvl = icsd_adt_alg_rtanc_get_wind_lvl,
};

struct adjdcc_function *ADJDCC_FUNC = (struct adjdcc_function *)(&ADJDCC_FUNC_t);

void icsd_adjdcc_demo()
{
    static int *alloc_addr = NULL;
    struct icsd_adjdcc_libfmt libfmt;
    struct icsd_adjdcc_infmt  fmt;
    icsd_adjdcc_get_libfmt(&libfmt, 0);
    dcc_printf("ADJDCC RAM SIZE:%d\n", libfmt.lib_alloc_size);
    if (alloc_addr == NULL) {
        alloc_addr = zalloc(libfmt.lib_alloc_size);
    }
    fmt.alloc_ptr = alloc_addr;
    icsd_adjdcc_set_infmt(&fmt, 0);
}

#endif

