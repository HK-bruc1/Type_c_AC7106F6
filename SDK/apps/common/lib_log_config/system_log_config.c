#include "app_config.h"



/**
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 */
/*-----------------------------------------------------------*/
const char log_tag_const_v_SYS_TMR  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SYS_TMR  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_SYS_TMR  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_SYS_TMR  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_SYS_TMR  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_JLFS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_JLFS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_JLFS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_JLFS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_JLFS  = CONFIG_DEBUG_LIB(1);

//FreeRTOS
const char log_tag_const_v_PORT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_PORT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_PORT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_PORT  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_PORT  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_KTASK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_KTASK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_KTASK  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_KTASK  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_KTASK  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_uECC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_uECC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_uECC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_uECC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_uECC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_HEAP_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_HEAP_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_HEAP_MEM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_HEAP_MEM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_HEAP_MEM  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_V_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_V_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_V_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_V_MEM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_V_MEM  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_P_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_P_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_P_MEM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_P_MEM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_P_MEM  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_P_MEM_C = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_P_MEM_C = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_P_MEM_C = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_P_MEM_C = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_P_MEM_C = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_PSRAM_HEAP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_PSRAM_HEAP = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_PSRAM_HEAP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_PSRAM_HEAP = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_PSRAM_HEAP = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_DEBUG_RECORD = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_DEBUG_RECORD = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_DEBUG_RECORD = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_DEBUG_RECORD = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_DEBUG_RECORD = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_DLOG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_DLOG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_DLOG  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_DLOG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_DLOG  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_JLDTP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_JLDTP = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_JLDTP = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_JLDTP = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_JLDTP = CONFIG_DEBUG_LIB(1);
