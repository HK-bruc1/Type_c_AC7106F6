#ifndef RDX_HOST_TEST_APP_CONFIG_H
#define RDX_HOST_TEST_APP_CONFIG_H

#define TCFG_RDX_ENABLE                         1
#define RDX_CFG_ONLINE_RECORDING_ENABLE         0
#define RDX_CFG_CONFERENCE_RECORDING_ENABLE     0
#define RDX_CFG_CALL_RECORDING_ENABLE           0
#define RDX_CFG_RECORD_PAUSE_RESUME_ENABLE      0
#define RDX_CFG_RECORD_MARK_ENABLE              0
#ifndef RDX_CFG_RTC_ENABLE
#define RDX_CFG_RTC_ENABLE                      0
#endif
#ifndef RDX_RTC_VALID_YEAR_MIN
#define RDX_RTC_VALID_YEAR_MIN                  2000
#endif
#ifndef RDX_RTC_VALID_YEAR_MAX
#define RDX_RTC_VALID_YEAR_MAX                  2099
#endif
#define RDX_CFG_BLE_TX_CAN_SEND_ENABLE          0
#define RDX_CFG_BLE_TX_DIAG_ENABLE              0
#define RDX_CFG_BLE_TX_QUEUE_DEPTH              4
#define RDX_CFG_BLE_TX_BURST_BUDGET             1

#endif
