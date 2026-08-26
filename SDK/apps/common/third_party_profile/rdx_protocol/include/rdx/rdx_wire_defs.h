#ifndef RDX_WIRE_DEFS_H
#define RDX_WIRE_DEFS_H

#include "rdx/domain/rdx_device_state_model.h"

/* APP -> device management commands. */
#define RDX_CMD_DL_AUTH_SN                      "*APP#authsn#"
#define RDX_CMD_DL_OFFTIME_CHECK                "*APP#cofftime#"
#define RDX_CMD_DL_BOUND                        "*APP#bound#"
#define RDX_CMD_DL_UNBOUND                      "*APP#unbound#"
#define RDX_CMD_DL_SET_DEFAULT                  "*APP#default#"
#define RDX_CMD_DL_BLE_NAME_QUERY               "*APP#cblename#"
#define RDX_CMD_DL_BLE_NAME_SET                 "*APP#blename#"
#define RDX_CMD_DL_MIC_GAIN_QUERY               "*APP#cmicgain#"
#define RDX_CMD_DL_MIC_GAIN_SET                 "*APP#micgain#"

/* Device -> APP management responses. */
#define RDX_CMD_UP_AUTH_SN                      "*DEV#authsn#"
#define RDX_CMD_UP_OFFTIME_CHECK                "*DEV#cofftime#"
#define RDX_CMD_UP_BOUND                        "*DEV#bound#"
#define RDX_CMD_UP_UNBOUND                      "*DEV#unbound#"
#define RDX_CMD_UP_SET_DEFAULT                  "*DEV#default#"
#define RDX_CMD_UP_BLE_NAME_QUERY               "*DEV#cblename#"
#define RDX_CMD_UP_BLE_NAME_SET                 "*DEV#blename#"
#define RDX_CMD_UP_MIC_GAIN_QUERY               "*DEV#cmicgain#"
#define RDX_CMD_UP_MIC_GAIN_SET                 "*DEV#micgain#"

/* BLE name wire/storage contract: printable ASCII, excluding '#'. */
#define RDX_BLE_NAME_MAX_LEN                    \
    RDX_DEVICE_STATE_BLE_NAME_MAX_LEN

/* RDX wire/VM range; ADC conversion stays in the selected adapter. */
#define RDX_MIC_GAIN_LEVEL_MAX                  \
    RDX_DEVICE_STATE_MIC_GAIN_MAX

/* Fixed-width text fields used by the auth/SN response. */
#define RDX_AUTH_KEY_STRING_SIZE                24
#define RDX_MAC_STRING_SIZE                     12
#define RDX_LABEL_SN_STRING_SIZE                16

#endif
