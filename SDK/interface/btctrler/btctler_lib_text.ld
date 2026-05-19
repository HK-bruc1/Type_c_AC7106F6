	    *(.ble_ll_const)
	    *(.ble_ll_code)

#if (defined CONFIG_CPU_BD45) || (defined CONFIG_CPU_BR56)
		*(.le_bb_ram_code)
		*(.le_ll_ram_code)
#endif /* (defined CONFIG_CPU_BD45) || (defined CONFIG_CPU_BR56) */

		. = ALIGN(4);
        tws_sync_call_begin = .;
        KEEP(*(.tws_sync_call))
        tws_sync_call_end = .;

		. = ALIGN(4);
        tws_func_stub_begin = .;
        KEEP(*(.tws_func_stub))
        tws_func_stub_end = .;

		. = ALIGN(4);
        tlcap_channel_begin = .;
        KEEP(*(.tlcap_channel))
        tlcap_channel_end = .;


