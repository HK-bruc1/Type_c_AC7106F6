#include "app_config.h"


/**
 * @brief Log (Verbose/Info/Debug/Warn/Error)
 */
/*-----------------------------------------------------------*/
const char log_tag_const_v_CLOCK  = CONFIG_DEBUG_LIB(0) ;
const char log_tag_const_i_CLOCK  = CONFIG_DEBUG_LIB(0) ;
const char log_tag_const_d_CLOCK  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_CLOCK  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_CLOCK  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LP_TIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LP_TIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LP_TIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LP_TIMER  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LP_TIMER  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LRC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LRC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_LRC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LRC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LRC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_RCH  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_RCH  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_RCH  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_RCH  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_RCH  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_P33_MISC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_P33_MISC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_P33_MISC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_P33_MISC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_P33_MISC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_P33  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_P33  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_P33  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_P33  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_P33  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_PMU  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_PMU  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_PMU  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_PMU  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_PMU  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_RTC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_RTC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_RTC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_RTC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_RTC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_WKUP  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_WKUP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_WKUP  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_WKUP  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_WKUP  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_SDFILE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SDFILE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_SDFILE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_SDFILE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_SDFILE  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_CHARGE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_CHARGE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_CHARGE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_CHARGE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_CHARGE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_c_CHARGE  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_DEBUG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_DEBUG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_DEBUG  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_DEBUG  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_DEBUG  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_PWM_LED  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_PWM_LED  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_PWM_LED  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_PWM_LED  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_PWM_LED  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_KEY  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_KEY  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_KEY  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_KEY  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_KEY  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_TMR  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_TMR  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_TMR  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_TMR  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_TMR  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_VM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_VM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_VM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_VM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_VM  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_TRIM_VDD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_TRIM_VDD  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_TRIM_VDD  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TRIM_VDD  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_TRIM_VDD  = CONFIG_DEBUG_LIB(1);

//audio dac
const char log_tag_const_v_SYS_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SYS_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_SYS_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_SYS_DAC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_SYS_DAC  = CONFIG_DEBUG_LIB(0);


const char log_tag_const_v_APP_EDET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_APP_EDET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_APP_EDET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_APP_EDET  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_APP_EDET  = CONFIG_DEBUG_LIB(0);


const char log_tag_const_v_FM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_FM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_FM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_FM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_FM  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_CORE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_CORE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_CORE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_CORE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_CORE  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_CACHE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_CACHE  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_CACHE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_CACHE  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_CACHE  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_LP_KEY  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LP_KEY  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LP_KEY  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_LP_KEY  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LP_KEY  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_LP_NFC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_LP_NFC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_LP_NFC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_LP_NFC  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_LP_NFC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_SD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SD  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_SD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_SD  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_SD  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_EAR_DETECT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_EAR_DETECT  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_EAR_DETECT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_EAR_DETECT  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_EAR_DETECT  = CONFIG_DEBUG_LIB(0);

const char log_tag_const_v_TDM  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_TDM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_d_TDM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_TDM  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_TDM  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_USB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_USB  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_USB  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_w_USB  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_USB  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_UART  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_UART  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_UART  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_UART  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_UART  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_SPI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_SPI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_SPI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_SPI  = CONFIG_DEBUG_LIB(1);
const char log_tag_const_e_SPI  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_GPTIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_GPTIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_GPTIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_GPTIMER  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_GPTIMER  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_PERI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_PERI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_PERI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_PERI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_PERI  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_GPADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_GPADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_GPADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_GPADC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_GPADC  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_EXTI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_EXTI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_EXTI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_EXTI  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_EXTI  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_GPIO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_GPIO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_GPIO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_GPIO  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_GPIO  = CONFIG_DEBUG_LIB(1);

const char log_tag_const_v_IIC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_i_IIC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_d_IIC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_w_IIC  = CONFIG_DEBUG_LIB(0);
const char log_tag_const_e_IIC  = CONFIG_DEBUG_LIB(1);


