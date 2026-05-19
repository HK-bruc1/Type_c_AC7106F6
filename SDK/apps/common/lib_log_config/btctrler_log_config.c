#include "app_config.h"

/*-----------------------------------------------------------*/

/**
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 */
/*-----------------------------------------------------------*/
//RF part
const char log_tag_const_v_Analog  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_Analog  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_Analog  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_Analog  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_Analog  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_RF  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_RF  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_RF  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_RF  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_RF  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_BDMGR   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_i_BDMGR   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_BDMGR   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_BDMGR   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_BDMGR   = CONFIG_DEBUG_LIB(1);

//Classic part
const char log_tag_const_v_HCI_LMP   = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_HCI_LMP   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_HCI_LMP   = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_HCI_LMP   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_HCI_LMP   = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LMP  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LMP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LMP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LMP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LMP  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LINK   = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LINK   = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LINK   = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LINK   = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_LINK   = CONFIG_DEBUG_LIB(0);

//LE part
const char log_tag_const_v_LE_BB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LE_BB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LE_BB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LE_BB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_LE_BB  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LE5_BB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LE5_BB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LE5_BB  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LE5_BB  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LE5_BB  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_HCI_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_HCI_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_HCI_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_HCI_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_HCI_LL  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_LL  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_E  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_E  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_E  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_E  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_E  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_M  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_M  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_M  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_M  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_M  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_ADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_ADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_ADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_ADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_ADV  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_SCAN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_SCAN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_SCAN  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_SCAN  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_SCAN  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_INIT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_INIT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_INIT  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_INIT  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_INIT  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_EXT_ADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_EXT_ADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_EXT_ADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_EXT_ADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_EXT_ADV  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_EXT_SCAN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_EXT_SCAN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_EXT_SCAN  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_EXT_SCAN  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_EXT_SCAN  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_EXT_INIT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_EXT_INIT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_EXT_INIT  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_EXT_INIT  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_EXT_INIT  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_TWS_ADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_TWS_ADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_TWS_ADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_TWS_ADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_TWS_ADV  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_TWS_SCAN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_TWS_SCAN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_TWS_SCAN  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_TWS_SCAN  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_TWS_SCAN  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_S  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_S  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LL_S  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_S  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_S  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_RL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_RL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LL_RL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_RL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_RL  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_WL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_WL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LL_WL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_WL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_WL  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_AES  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_AES  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_AES  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_AES  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_AES  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_AES128  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_i_AES128  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_AES128  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_AES128  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_AES128  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_LL_PADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_PADV  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_PADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_PADV  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_PADV  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_DX  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_DX  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_DX  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_DX  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_DX  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_PHY  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_PHY  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_PHY  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_PHY  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_PHY  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_AFH  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_AFH  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LL_AFH  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LL_AFH  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_AFH  = CONFIG_DEBUG_LIB(1);

//HCI part
const char log_tag_const_v_Thread  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_Thread  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_Thread  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_Thread  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_Thread  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_HCI_STD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_HCI_STD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_HCI_STD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_HCI_STD  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_HCI_STD  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_HCI_LL5  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_HCI_LL5  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_HCI_LL5  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_HCI_LL5  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_HCI_LL5  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_ISO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_ISO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_ISO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LL_ISO  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_ISO  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_BIS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_BIS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_BIS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_BIS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_BIS  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LL_CIS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_CIS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_CIS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LL_CIS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LL_CIS  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_BL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_BL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_BL  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_BL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_BL  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_c_BL  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_TWS_LE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_i_TWS_LE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_TWS_LE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TWS_LE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_TWS_LE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_c_TWS_LE  = CONFIG_DEBUG_LIB(1);


const char log_tag_const_v_TWS_LMP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_i_TWS_LMP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_TWS_LMP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TWS_LMP  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_TWS_LMP  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_TWS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_TWS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_TWS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TWS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_TWS  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_TWS_DATA_TRANS  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_TWS_DATA_TRANS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_TWS_DATA_TRANS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TWS_DATA_TRANS  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_TWS_DATA_TRANS  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_TWS_ESCO  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_i_TWS_ESCO  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_TWS_ESCO  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TWS_ESCO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_TWS_ESCO  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_QUICK_CONN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_QUICK_CONN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_QUICK_CONN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_QUICK_CONN  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_QUICK_CONN  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_LL_PC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LL_PC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LL_PC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LL_PC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_LL_PC  = CONFIG_DEBUG_LIB(1);

