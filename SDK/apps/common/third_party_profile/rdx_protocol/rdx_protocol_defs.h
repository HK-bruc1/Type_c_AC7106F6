#ifndef RDX_PROTOCOL_DEFS_H
#define RDX_PROTOCOL_DEFS_H

/* APP -> device read-only management commands. */
#define RDX_CMD_DL_AUTH_SN                      "*APP#authsn#"
#define RDX_CMD_DL_OFFTIME_CHECK                "*APP#cofftime#"

/* Device -> APP read-only management responses. */
#define RDX_CMD_UP_AUTH_SN                      "*DEV#authsn#"
#define RDX_CMD_UP_OFFTIME_CHECK                "*DEV#cofftime#"

/* Fixed-width text fields used by the auth/SN response. */
#define RDX_AUTH_KEY_STRING_SIZE                24
#define RDX_MAC_STRING_SIZE                     12
#define RDX_LABEL_SN_STRING_SIZE                16

/*
 * Device Ability is a 32-bit wire bitfield. Manufacturer Data serializes it
 * low byte first; reserved bits must remain zero.
 */
#define RDX_ABILITY_NONE                        0x00000000UL
#define RDX_ABILITY_MULTIMEDIA_RECORDING        (1UL << 0)
#define RDX_ABILITY_RTC                         (1UL << 1)
#define RDX_ABILITY_CLASSIC_BT                  (1UL << 2)
#define RDX_ABILITY_CONFERENCE_NOISE_REDUCTION  (1UL << 3)
#define RDX_ABILITY_CALL_RECORDING              (1UL << 4)
#define RDX_ABILITY_CONFERENCE_RECORDING        (1UL << 5)
#define RDX_ABILITY_WIFI_AP                     (1UL << 6)
#define RDX_ABILITY_LOCAL_STORAGE               (1UL << 7)
#define RDX_ABILITY_RECORD_PAUSE_RESUME         (1UL << 9)
#define RDX_ABILITY_RECORD_MARK                 (1UL << 10)
#define RDX_ABILITY_FLASHNOTE                   (1UL << 11)
#define RDX_ABILITY_NETWORK                     (1UL << 12)
#define RDX_ABILITY_CHILD_PARENT                (1UL << 13)
#define RDX_ABILITY_WIFI_AP_V2                  (1UL << 14)
#define RDX_ABILITY_LEFT_RIGHT_1V1              (1UL << 15)

#endif
