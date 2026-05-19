/*********************************************************************************************
    *   Filename        : log_config.c

    *   Description     : Optimized Code & RAM (编译优化Log配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-18 14:45

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/

/**
 * @brief Bluetooth Controller Log
 */
/*-----------------------------------------------------------*/
const char log_tag_const_v_SETUP  = 0;
const char log_tag_const_i_SETUP  = 0;
const char log_tag_const_w_SETUP  = 0;
const char log_tag_const_d_SETUP  = 1;
const char log_tag_const_e_SETUP  = 1;

const char log_tag_const_v_BOARD  = 0;
const char log_tag_const_i_BOARD  = 1;
const char log_tag_const_d_BOARD  = 1;
const char log_tag_const_w_BOARD  = 1;
const char log_tag_const_e_BOARD  = 1;

const char log_tag_const_v_EARPHONE   = 0;
const char log_tag_const_i_EARPHONE   = 1;
const char log_tag_const_d_EARPHONE   = 0;
const char log_tag_const_w_EARPHONE   = 1;
const char log_tag_const_e_EARPHONE   = 1;

const char log_tag_const_v_UI  = 0;
const char log_tag_const_i_UI  = 1;
const char log_tag_const_d_UI  = 0;
const char log_tag_const_w_UI  = 1;
const char log_tag_const_e_UI  = 1;

const char log_tag_const_v_APP_CHARGE  = 0;
const char log_tag_const_i_APP_CHARGE  = 1;
const char log_tag_const_d_APP_CHARGE  = 0;
const char log_tag_const_w_APP_CHARGE  = 1;
const char log_tag_const_e_APP_CHARGE  = 1;

const char log_tag_const_v_APP_ANCBOX  = 0;
const char log_tag_const_i_APP_ANCBOX  = 1;
const char log_tag_const_d_APP_ANCBOX  = 0;
const char log_tag_const_w_APP_ANCBOX  = 1;
const char log_tag_const_e_APP_ANCBOX  = 1;

const char log_tag_const_v_KEY_EVENT_DEAL  = 0;
const char log_tag_const_i_KEY_EVENT_DEAL  = 1;
const char log_tag_const_d_KEY_EVENT_DEAL  = 0;
const char log_tag_const_w_KEY_EVENT_DEAL  = 1;
const char log_tag_const_e_KEY_EVENT_DEAL  = 1;

const char log_tag_const_v_APP_CHARGESTORE  = 0;
const char log_tag_const_i_APP_CHARGESTORE  = 1;
const char log_tag_const_d_APP_CHARGESTORE  = 0;
const char log_tag_const_w_APP_CHARGESTORE  = 1;
const char log_tag_const_e_APP_CHARGESTORE  = 1;

const char log_tag_const_v_APP_UMIDIGI_CHARGESTORE  = 0;
const char log_tag_const_i_APP_UMIDIGI_CHARGESTORE  = 1;
const char log_tag_const_d_APP_UMIDIGI_CHARGESTORE  = 0;
const char log_tag_const_w_APP_UMIDIGI_CHARGESTORE  = 1;
const char log_tag_const_e_APP_UMIDIGI_CHARGESTORE  = 1;

const char log_tag_const_v_APP_TESTBOX  = 0;
const char log_tag_const_i_APP_TESTBOX  = 1;
const char log_tag_const_d_APP_TESTBOX  = 0;
const char log_tag_const_w_APP_TESTBOX  = 1;
const char log_tag_const_e_APP_TESTBOX  = 1;

const char log_tag_const_v_APP_CHARGE_CALIBRATION = 0;
const char log_tag_const_i_APP_CHARGE_CALIBRATION = 1;
const char log_tag_const_d_APP_CHARGE_CALIBRATION = 0;
const char log_tag_const_w_APP_CHARGE_CALIBRATION = 1;
const char log_tag_const_e_APP_CHARGE_CALIBRATION = 1;

const char log_tag_const_v_APP_IDLE  = 0;
const char log_tag_const_i_APP_IDLE  = 1;
const char log_tag_const_d_APP_IDLE  = 0;
const char log_tag_const_w_APP_IDLE  = 1;
const char log_tag_const_e_APP_IDLE  = 1;

const char log_tag_const_v_APP_POWER  = 0;
const char log_tag_const_i_APP_POWER  = 1;
const char log_tag_const_d_APP_POWER  = 0;
const char log_tag_const_w_APP_POWER  = 1;
const char log_tag_const_e_APP_POWER  = 1;

const char log_tag_const_v_APP  = 0;
const char log_tag_const_i_APP  = 1;
const char log_tag_const_d_APP  = 0;
const char log_tag_const_w_APP  = 1;
const char log_tag_const_e_APP  = 1;

const char log_tag_const_v_USER_CFG  = 0;
const char log_tag_const_i_USER_CFG  = 1;
const char log_tag_const_d_USER_CFG  = 0;
const char log_tag_const_w_USER_CFG  = 1;
const char log_tag_const_e_USER_CFG  = 1;

const char log_tag_const_v_APP_TONE  = 0;
const char log_tag_const_i_APP_TONE  = 1;
const char log_tag_const_d_APP_TONE  = 0;
const char log_tag_const_w_APP_TONE  = 1;
const char log_tag_const_e_APP_TONE  = 1;

const char log_tag_const_v_BT_TWS  = 0;
const char log_tag_const_i_BT_TWS  = 1;
const char log_tag_const_d_BT_TWS  = 0;
const char log_tag_const_w_BT_TWS  = 1;
const char log_tag_const_e_BT_TWS  = 1;

const char log_tag_const_v_AEC_USER  = 0;
const char log_tag_const_i_AEC_USER  = 1;
const char log_tag_const_d_AEC_USER  = 0;
const char log_tag_const_w_AEC_USER  = 1;
const char log_tag_const_e_AEC_USER  = 1;

const char log_tag_const_v_CVP_USER  = 0;
const char log_tag_const_i_CVP_USER  = 1;
const char log_tag_const_d_CVP_USER  = 0;
const char log_tag_const_w_CVP_USER  = 1;
const char log_tag_const_e_CVP_USER  = 1;

const char log_tag_const_v_BT_BLE  = 0;
const char log_tag_const_i_BT_BLE  = 0;
const char log_tag_const_d_BT_BLE  = 1;
const char log_tag_const_w_BT_BLE  = 1;
const char log_tag_const_e_BT_BLE  = 1;

const char log_tag_const_v_EARTCH_EVENT_DEAL  = 0;
const char log_tag_const_i_EARTCH_EVENT_DEAL  = 1;
const char log_tag_const_d_EARTCH_EVENT_DEAL  = 0;
const char log_tag_const_w_EARTCH_EVENT_DEAL  = 1;
const char log_tag_const_e_EARTCH_EVENT_DEAL  = 1;

const char log_tag_const_v_ONLINE_DB  = 0;
const char log_tag_const_i_ONLINE_DB  = 0;
const char log_tag_const_d_ONLINE_DB  = 0;
const char log_tag_const_w_ONLINE_DB  = 0;
const char log_tag_const_e_ONLINE_DB  = 1;

const char log_tag_const_v_MUSIC  = 0;
const char log_tag_const_i_MUSIC  = 1;
const char log_tag_const_w_MUSIC  = 0;
const char log_tag_const_d_MUSIC  = 1;
const char log_tag_const_e_MUSIC  = 1;

const char log_tag_const_v_APP_MUSIC  = 0;
const char log_tag_const_i_APP_MUSIC  = 1;
const char log_tag_const_d_APP_MUSIC  = 0;
const char log_tag_const_w_APP_MUSIC  = 1;
const char log_tag_const_e_APP_MUSIC  = 1;

const char log_tag_const_v_PC  = 0;
const char log_tag_const_i_PC  = 1;
const char log_tag_const_w_PC  = 0;
const char log_tag_const_d_PC  = 1;
const char log_tag_const_e_PC  = 1;

const char log_tag_const_v_WIRELESSMIC  = 0;
const char log_tag_const_i_WIRELESSMIC  = 1;
const char log_tag_const_w_WIRELESSMIC  = 0;
const char log_tag_const_d_WIRELESSMIC  = 1;
const char log_tag_const_e_WIRELESSMIC  = 1;

const char log_tag_const_v_KWS_VOICE_EVENT  = 0;
const char log_tag_const_i_KWS_VOICE_EVENT  = 1;
const char log_tag_const_d_KWS_VOICE_EVENT  = 1;
const char log_tag_const_w_KWS_VOICE_EVENT  = 1;
const char log_tag_const_e_KWS_VOICE_EVENT  = 1;

const char log_tag_const_v_APP_CFG_TOOL  = 0;
const char log_tag_const_i_APP_CFG_TOOL  = 1;
const char log_tag_const_d_APP_CFG_TOOL  = 0;
const char log_tag_const_w_APP_CFG_TOOL  = 1;
const char log_tag_const_e_APP_CFG_TOOL  = 1;

const char log_tag_const_v_NET_IFLY = 0;
const char log_tag_const_i_NET_IFLY = 1;
const char log_tag_const_d_NET_IFLY = 0;
const char log_tag_const_w_NET_IFLY = 1;
const char log_tag_const_e_NET_IFLY = 1;

const char log_tag_const_v_GSENSOR  = 0;
const char log_tag_const_i_GSENSOR  = 1;
const char log_tag_const_d_GSENSOR  = 0;
const char log_tag_const_w_GSENSOR  = 1;
const char log_tag_const_e_GSENSOR  = 1;


