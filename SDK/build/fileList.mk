




























c_SRC_FILES += audio/framework/plugs/source/a2dp_file.c audio/framework/plugs/source/a2dp_streamctrl.c audio/framework/plugs/source/esco_file.c audio/framework/plugs/source/adc_file.c audio/framework/nodes/esco_tx_node.c audio/framework/nodes/plc_node.c audio/framework/nodes/volume_node.c audio/framework/node_list.c
c_SRC_FILES += audio/framework/nodes/cvp_dms_node.c
c_SRC_FILES += audio/framework/nodes/cvp_v3_node.c
c_SRC_FILES += audio/common/audio_node_config.c audio/common/audio_dvol.c audio/common/audio_general.c audio/common/audio_build_needed.c audio/common/audio_plc.c audio/common/audio_noise_gate.c audio/common/audio_ns.c audio/common/audio_utils.c audio/common/amplitude_statistic.c audio/common/frame_length_adaptive.c audio/common/bt_audio_energy_detection.c audio/common/audio_event_handler.c audio/common/debug/audio_debug.c audio/common/power/mic_power_manager.c audio/common/audio_volume_mixer.c audio/common/audio_effect_verify.c audio/common/pcm_data/sine_pcm.c
c_SRC_FILES += audio/common/demo/audio_demo.c
c_SRC_FILES += audio/common/demo/hw_math_v2_demo.c
c_SRC_FILES += audio/interface/player/tone_player.c audio/interface/player/ring_player.c audio/interface/player/a2dp_player.c audio/interface/player/esco_player.c audio/interface/player/key_tone_player.c audio/interface/player/dev_flow_player.c audio/interface/player/adda_loop_player.c
c_SRC_FILES += audio/interface/recoder/esco_recoder.c audio/interface/recoder/dev_flow_recoder.c
c_SRC_FILES += audio/interface/user_defined/audio_dsp_low_latency_player.c audio/interface/user_defined/env_noise_recoder.c
c_SRC_FILES += audio/effect/eq_config.c audio/effect/audio_dc_offset_remove.c audio/effect/effects_adj.c audio/effect/effects_dev.c audio/effect/effects_default_param.c audio/effect/node_param_update.c
c_SRC_FILES += audio/CVP/audio_aec.c audio/CVP/audio_cvp.c audio/CVP/audio_cvp_dms.c audio/CVP/audio_cvp_online.c audio/CVP/audio_cvp_ref_task.c audio/CVP/audio_cvp_config.c
c_SRC_FILES += audio/CVP/audio_cvp_v3.c
c_SRC_FILES += audio/framework/plugs/source/linein_file.c



c_SRC_FILES += audio/framework/plugs/source/pc_spk_file.c audio/interface/player/pc_spk_player.c






c_SRC_FILES += audio/framework/nodes/pc_mic_node.c audio/interface/recoder/pc_mic_recoder.c
c_SRC_FILES += audio/cpu/common.c
c_SRC_FILES += apps/common/lib_version/version_check.c apps/common/config/bt_profile_config.c



c_SRC_FILES += apps/common/lib_log_config/btctrler_log_config.c apps/common/lib_log_config/btstack_log_config.c apps/common/lib_log_config/driver_log_config.c apps/common/lib_log_config/media_log_config.c apps/common/lib_log_config/net_log_config.c apps/common/lib_log_config/system_log_config.c apps/common/lib_log_config/update_log_config.c
c_SRC_FILES += apps/common/debug/debug_uart_config.c
c_SRC_FILES += apps/common/fat_nor/cfg_private.c




c_SRC_FILES += apps/common/debug/memory_debug.c
c_SRC_FILES += apps/common/third_party_profile/multi_protocol_main.c apps/common/third_party_profile/multi_protocol_common.c apps/common/third_party_profile/multi_protocol_event.c
c_SRC_FILES += apps/common/third_party_profile/rdx_protocol/rdx_protocol_entry.c apps/common/third_party_profile/rdx_protocol/rdx_ble_transport_br56.c apps/common/third_party_profile/rdx_protocol/rdx_device_management.c apps/common/third_party_profile/rdx_protocol/rdx_device_state.c apps/common/third_party_profile/rdx_protocol/rdx_identity.c apps/common/third_party_profile/rdx_protocol/rdx_platform_br56.c apps/common/third_party_profile/rdx_protocol/rdx_mvp0_protocol.c apps/common/third_party_profile/rdx_protocol/rdx_appkey_verifier.c apps/common/third_party_profile/rdx_protocol/rdx_rtc.c
THIRD_PARTY_LIBS += apps/common/third_party_profile/rdx_protocol/librdxApp.a
c_SRC_FILES += apps/common/jldtp/uart_transport.c apps/common/jldtp/jldtp_manager.c
c_SRC_FILES += apps/common/device/key/key_driver.c
c_SRC_FILES += apps/common/device/usb/usb_config.c apps/common/device/usb/device/descriptor.c apps/common/device/usb/device/usb_device.c apps/common/device/usb/device/user_setup.c apps/common/device/usb/device/task_pc.c
c_SRC_FILES += apps/common/device/usb/device/usb_pll_trim.c
c_SRC_FILES += apps/common/device/usb/device/msd_upgrade.c
c_SRC_FILES += apps/common/device/usb/device/hid.c
c_SRC_FILES += apps/common/device/usb/device/uac_stream.c



c_SRC_FILES += apps/common/device/usb/device/uac1.c
c_SRC_FILES += apps/common/device/usb/usb_epbuf_manager.c apps/common/device/usb/usb_task.c
c_SRC_FILES += cpu/components/iic_soft.c cpu/components/iic_api.c cpu/components/ir_encoder.c cpu/components/ir_decoder.c cpu/components/led_spi.c cpu/components/rdec_soft.c
c_SRC_FILES += cpu/config/gpio_file_parse.c cpu/config/lib_power_config.c
c_SRC_FILES += audio/cpu/br56/audio_setup.c audio/cpu/br56/audio_mic_capless.c audio/cpu/br56/audio_config.c audio/cpu/br56/audio_configs_dump.c





c_SRC_FILES += audio/cpu/br56/audio_dai/audio_pdm.c



c_SRC_FILES += audio/cpu/br56/audio_anc_platform.c





c_SRC_FILES += audio/cpu/br56/audio_accelerator/hw_fft.c



c_SRC_FILES += audio/cpu/br56/audio_demo/audio_adc_demo.c




c_SRC_FILES += audio/cpu/br56/audio_demo/audio_dac_demo.c audio/cpu/br56/audio_demo/audio_fft_demo.c #audio/cpu/br56/audio_demo/audio_alink_demo.c #audio/cpu/br56/audio_demo/audio_pdm_demo.c
c_SRC_FILES += cpu/br56/setup.c cpu/br56/overlay_code.c cpu/br56/rf_api.c




c_SRC_FILES += cpu/br56/charge/charge_ocp.c



c_SRC_FILES += cpu/br56/charge/charge.c cpu/br56/charge/charge_config.c
c_SRC_FILES += cpu/br56/power/power_port.c cpu/br56/power/power_gate.c cpu/br56/power/key_wakeup.c cpu/br56/power/power_app.c cpu/br56/power/power_config.c






c_SRC_FILES += cpu/br56/ui_driver/led7/led7_driver.c cpu/br56/ui_driver/ui_common.c
c_SRC_FILES += apps/earphone/mode/pc/pc.c apps/earphone/mode/pc/pc_key_msg_table.c
c_SRC_FILES += apps/earphone/demo/pbg_demo.c
c_SRC_FILES += apps/earphone/mode/bt/poweroff.c apps/earphone/mode/bt/dual_conn.c apps/earphone/mode/bt/phone_call.c
c_SRC_FILES += apps/earphone/mode/bt/a2dp_play.c
c_SRC_FILES += apps/earphone/mode/bt/tws_pair_by_chip_conn.c
