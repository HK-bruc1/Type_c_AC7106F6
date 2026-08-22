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

/*
 * Stage-2C bounded PCM Bridge Spike.  It consumes the existing passive Tap,
 * normalizes Near/Far to 16 kHz mono Q15, and produces 20 ms mixed PCM frames.
 * The diagnostic build does not attach Encoder, AI_TX, RDX Protocol, or BLE.
 */
#define TCFG_USB_CALL_AUDIO_BRIDGE_DIAG_ENABLE       0
#define TCFG_USB_CALL_AUDIO_ENCODER_DIAG_ENABLE      0
#define USB_CALL_AUDIO_BRIDGE_OUTPUT_SAMPLE_RATE     16000
#define USB_CALL_AUDIO_BRIDGE_FRAME_MS                  20
#define USB_CALL_AUDIO_BRIDGE_BUFFER_MS                 80
#define USB_CALL_AUDIO_BRIDGE_PRELOAD_MS                40
#define USB_CALL_AUDIO_BRIDGE_REPORT_MS               5000
#define USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_MS            100

#if (TCFG_USB_CALL_AUDIO_TAP_DIAG_GATE_MASK & ~0x03)
#error "USB call Tap diagnostic gate mask is invalid"
#endif

#if (USB_CALL_AUDIO_TAP_DIAG_PERIOD_MS < 200)
#error "USB call Tap diagnostic period is too short"
#endif

#if (USB_CALL_AUDIO_TAP_DIAG_SAMPLE_DECIMATION < 1)
#error "USB call Tap diagnostic decimation must be positive"
#endif

#if TCFG_USB_CALL_AUDIO_TAP_DIAG_ENABLE \
    && TCFG_USB_CALL_AUDIO_BRIDGE_DIAG_ENABLE
#error "USB call Tap and Bridge diagnostics cannot own the Tap together"
#endif

#if TCFG_USB_CALL_AUDIO_ENCODER_DIAG_ENABLE \
    && (TCFG_USB_CALL_AUDIO_TAP_DIAG_ENABLE \
        || TCFG_USB_CALL_AUDIO_BRIDGE_DIAG_ENABLE)
#error "USB call Encoder diagnostic must exclusively own the Bridge"
#endif

#if TCFG_USB_CALL_AUDIO_ENCODER_DIAG_ENABLE \
    && (!TCFG_SOURCE_DEV1_NODE_ENABLE \
        || !TCFG_ENCODER_NODE_ENABLE \
        || !TCFG_AI_TX_NODE_ENABLE \
        || !TCFG_ENC_OPUS_ENABLE)
#error "USB call Encoder diagnostic requires SourceDev1, Encoder, AI_TX and Opus"
#endif

#if (USB_CALL_AUDIO_BRIDGE_FRAME_MS != 20)
#error "USB call Bridge Spike requires 20 ms output frames"
#endif

#if (USB_CALL_AUDIO_BRIDGE_PRELOAD_MS < USB_CALL_AUDIO_BRIDGE_FRAME_MS) \
    || (USB_CALL_AUDIO_BRIDGE_PRELOAD_MS > USB_CALL_AUDIO_BRIDGE_BUFFER_MS)
#error "USB call Bridge preload must fit in the bounded buffer"
#endif

#if (USB_CALL_AUDIO_BRIDGE_PRELOAD_MS + USB_CALL_AUDIO_BRIDGE_FRAME_MS \
     > USB_CALL_AUDIO_BRIDGE_BUFFER_MS)
#error "USB call Bridge needs one master frame above the preload level"
#endif

#if (USB_CALL_AUDIO_BRIDGE_BUFFER_MS < 40) \
    || (USB_CALL_AUDIO_BRIDGE_BUFFER_MS > 80)
#error "USB call Bridge buffer must stay within the Stage-2C 40-80 ms bound"
#endif

#if (USB_CALL_AUDIO_BRIDGE_REPORT_MS < 200)
#error "USB call Bridge diagnostic period is too short"
#endif

#if (USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_MS < USB_CALL_AUDIO_BRIDGE_FRAME_MS) \
    || (USB_CALL_AUDIO_BRIDGE_SOURCE_LOST_MS \
        % USB_CALL_AUDIO_BRIDGE_FRAME_MS)
#error "USB call Bridge source-lost threshold must align to output frames"
#endif

#endif
