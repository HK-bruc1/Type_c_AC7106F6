#ifndef USB_CALL_AUDIO_TAP_CONFIG_H
#define USB_CALL_AUDIO_TAP_CONFIG_H

/*
 * Stage-1 hardware diagnostic only.  This switch opens the internal Near/Far
 * Tap gates and installs a statistics consumer; it does not advertise Call
 * Recording Ability, encode audio, or send PCM to RDX/BLE.
 *
 * Set this back to 0 after collecting the Stage-1 evidence.
 */
#define TCFG_USB_CALL_AUDIO_TAP_DIAG_ENABLE          0
#define TCFG_USB_CALL_AUDIO_TAP_DIAG_GATE_MASK       0x03
#define USB_CALL_AUDIO_TAP_DIAG_PERIOD_MS            1000
#define USB_CALL_AUDIO_TAP_DIAG_SAMPLE_DECIMATION       8

#if (TCFG_USB_CALL_AUDIO_TAP_DIAG_GATE_MASK & ~0x03)
#error "USB call Tap diagnostic gate mask is invalid"
#endif

#if (USB_CALL_AUDIO_TAP_DIAG_PERIOD_MS < 200)
#error "USB call Tap diagnostic period is too short"
#endif

#if (USB_CALL_AUDIO_TAP_DIAG_SAMPLE_DECIMATION < 1)
#error "USB call Tap diagnostic decimation must be positive"
#endif

#endif
