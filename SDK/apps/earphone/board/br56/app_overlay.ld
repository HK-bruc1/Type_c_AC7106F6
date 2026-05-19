
        /*不使用动态加载时，使用overlay*/
#ifndef CONFIG_CODE_MOVABLE_ENABLE
	OVERLAY : AT(0x200000) SUBALIGN(4)
    {
		.overlay_init
		{
#if !(TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))
            _initcall_begin = .;
            PROVIDE(initcall_begin = .);
            KEEP(*(.initcall))
            _initcall_end = .;
            PROVIDE(initcall_end = .);

            _early_initcall_begin = .;
            PROVIDE(early_initcall_begin = .);
            KEEP(*(.early.initcall))
            _early_initcall_end = .;
            PROVIDE(early_initcall_end = .);

            _late_initcall_begin = .;
            PROVIDE(late_initcall_begin = .);
            KEEP(*(.late.initcall))
            _late_initcall_end = .;
            PROVIDE(late_initcall_end = .);

            _platform_initcall_begin = .;
            PROVIDE(platform_initcall_begin = .);
            KEEP(*(.platform.initcall))
            _platform_initcall_end = .;
            PROVIDE(platform_initcall_end = .);

            _module_initcall_begin = .;
            PROVIDE(module_initcall_begin = .);
            KEEP(*(.module.initcall))
            _module_initcall_end = .;
            PROVIDE(module_initcall_end = .);

            *(.initcall.bank.text)
            *(.rf_init.bank1.text)
            *(.rf_init.bank1.text.const)
            *(.rf_init.bank2.text)
#endif
        }
		.overlay_aec
		{
            aec_code_begin = . ;
#if AUDIO_CVP_TEXT_AT_RAM
	        . = ALIGN(4);
			*(.text._*)
#endif
#if AUDIO_CVP_AEC_AT_RAM
            *(.jlsp_aec_code)
			*(.jlsp_aec_const)
#endif
#if AUDIO_CVP_NLP_AT_RAM
            *(.nlp_code)
            *(.nlp_const)
		    *(.qmf_code)
		    *(.qmf_const)
            *(.jlsp_nlp_code)
			*(.jlsp_nlp_const)
#endif
#if AUDIO_CVP_NS_AT_RAM
			*(.ns_code)
			*(.ns_const)
#endif
#if AUDIO_CVP_COMMON_AT_RAM
			. = ALIGN(4);
			*(.aec_mem)
            *(.aec_mux)
			*(.aec_code)
			*(.aec_const)
			*(.res_code)
			*(.res_const)
            *(.bark_const)
            *(.der_code)
            *(.der_const)
#endif
#if AUDIO_CVP_DNS_AT_RAM
	        . = ALIGN(4);
            *(.dns_common_data)
            *(.dns_param_data_single)
            *(.dns_param_data_dual)
	        . = ALIGN(4);
#endif
#if AUDIO_CVP_AGC_AT_RAM
            *(.agc_code)
            *(.agc_const)
            *(.jlsp_agc_code)
            *(.jlsp_agc_const)
#endif
#if AUDIO_CVP_DMS_AT_RAM
            *(.dms_code)
            *(.dms_const)
            *(.jlsp_enc_code)
            *(.jlsp_enc_const)
            *(.jlsp_dms_hybrid_code)
            *(.jlsp_dms_hybrid_const)
			*(.jlsp_dms_awn_code)
            *(.jlsp_dms_awn_const)
#endif
#if AUDIO_CVP_SMS_AT_RAM
            *(.sms_const)
            *(.sms_code)
            *(.sms_stereo_const)
            *(.sms_stereo_code)
#endif
#if AUDIO_CVP_PREP_AT_RAM
            *(.jlsp_prep_code)
            *(.jlsp_prep_const)
#endif
#if AUDIO_CVP_WN_AT_RAM
            *(.jlsp_wn_code)
            *(.jlsp_wn_const)
#endif
#if AUDIO_CVP_THIRD_AT_RAM
            *(.jlsp_tri_code)
            *(.jlsp_tri_const)
#endif
			aec_code_end = . ;
			aec_code_size = aec_code_end - aec_code_begin ;
#if AUDIO_MSBC_CODEC_AT_RAM
            *(.msbc_enc)
            *(.msbc_code)
            *(.msbc_const)
            *(.msbc_codec_code)
            *(.msbc_codec_const)
#endif
#if AUDIO_CVSD_CODEC_AT_RAM
            *(.cvsd_codec)
            *(.cvsd_code)
            *(.cvsd_const)
#endif

		}
		.overlay_aac
		{
#if AUD_AAC_DEC_AT_RAM
			aac_dec_code_begin = .;
			*(.bt_aac_dec_code)
			*(.bt_aac_dec_sparse_code)
			aac_dec_code_end = .;
			aac_dec_code_size  = aac_dec_code_end - aac_dec_code_begin ;

			. = ALIGN(4);
			bt_aac_dec_const_begin = .;
			*(.bt_aac_dec_const)
			*(.bt_aac_dec_sparse_const)
			bt_aac_dec_const_end = .;
			bt_aac_dec_const_size = bt_aac_dec_const_end -  bt_aac_dec_const_begin ;

			. = ALIGN(4);
			*(.aac_mem)
			*(.aac_ctrl_mem)
#endif
		}
        /*
        .overlay_lc3
        {
            lc3_dec_code_begin = .;
            *(.lc3_dec_code)
            lc3_dec_code_end = .;
            lc3_dec_code_size = lc3_dec_code_end - lc3_dec_code_begin;

            . = ALIGN(4);
            lc3_dec_const_begin = .;
            *(.lc3_dec_const)
            lc3_dec_const_end = .;
            lc3_dec_const_size = lc3_dec_const_end - lc3_dec_const_begin;
            *(.lc3_dec_data)
            *(.lc3_dec_bss)
        }
        */

		.overlay_mp3
		{
			*(.mp3_mem)
			*(.mp3_ctrl_mem)
			*(.mp3pick_mem)
			*(.mp3pick_ctrl_mem)
			*(.dec2tws_mem)
		}
		.overlay_wma
		{
			*(.wma_mem)
			*(.wma_ctrl_mem)
			*(.wmapick_mem)
			*(.wmapick_ctrl_mem)
		}
		.overlay_wav
		{
			*(.wav_mem)
			*(.wav_ctrl_mem)
		}
		.overlay_ape
        {
            *(.ape_mem)
            *(.ape_ctrl_mem)
		}
		.overlay_flac
        {
            *(.flac_mem)
            *(.flac_ctrl_mem)
		}
		.overlay_m4a
        {
            *(.m4a_mem)
            *(.m4a_ctrl_mem)
		}
		.overlay_amr
        {
            *(.amr_mem)
            *(.amr_ctrl_mem)
		}
		.overlay_dts
        {
            *(.dts_mem)
            *(.dts_ctrl_mem)
		}
		.overlay_fm
		{
			*(.fm_mem)
		}
        .overlay_pc
		{
		}

    } > ram0

#endif
