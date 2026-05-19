// *INDENT-OFF*
#include "app_config.h"
#include "audio_config_def.h"

/* ==================================  BR56 SDK memory ============================================

 _______________ ___ RAM_LIMIT_H                           _______________ ___ RAM_LIMIT_H
|     HEAP1     |                                         |     HEAP      |
|_______________|___ data_code_pc_limit_H                 |_______________|___ data_code_pc_limit_H
| audio overlay |                                         | audio overlay |
|_______________|                                         |_______________|
|   data_code   |                                         |   data_code   |
|_______________|___ data_code_pc_limit_L                 |_______________|___ data_code_pc_limit_L
|     bss       |                                         |     bss       |
|_______________|                                         |_______________|
|     data      |                                         |     data      |
|_______________|                                         |_______________|
|   irq_stack   |                                         |     TLB       |
|_______________|                                         |_______________|
|   boot info   |                                         | os data/code  |
|_______________|                                         |_______________|
|     TLB       |                                         |   irq_stack   |
|_______________|                                         |_______________|
|     HEAP      |                                         |   boot info   |
|_______________|___ RAM_LIMIT_L                          |_______________|___ RAM_LIMIT_L
|    update     |                                         |    update     |
|_______________|                                         |_______________|
|rom export ram |                                         |rom export ram |
|_______________|___ 0x100200                             |_______________|___ 0x100200
|   isr base    |                                         |   isr base    |
|_______________|___ 0x100000                             |_______________|___ 0x100000

 =============================================================================================== */

#include "maskrom_stubs.ld"

EXTERN(
_start
#include "sdk_used_list.c"
);

UPDATA_SIZE     = 0x200;
//UPDATA_BEG      = 0x130000 - UPDATA_SIZE;
UPDATA_BEG      = _MASK_EXPORT_MEM_BEGIN - UPDATA_SIZE;
UPDATA_BREDR_BASE_BEG = 0x2F0000;

//RAM_LIMIT_L     =_MASK_EXPORT_MEM_BEGIN  + _MASK_EXPORT_MEM_SIZE;
RAM_LIMIT_L     = _UBOOT_LOADER_RAM_START;
RAM_LIMIT_H     = UPDATA_BEG;
PHY_RAM_SIZE    = RAM_LIMIT_H - RAM_LIMIT_L;

//from mask export
ISR_BASE       = _IRQ_MEM_ADDR;
ROM_RAM_SIZE   = _MASK_EXPORT_MEM_SIZE;// _MASK_MEM_SIZE;
ROM_RAM_BEG    = _MASK_EXPORT_MEM_BEGIN;//_MASK_MEM_BEGIN;

RAM0_BEG 		= RAM_LIMIT_L;
RAM0_END 		= RAM_LIMIT_H;
RAM0_SIZE 		= RAM0_END - RAM0_BEG;

//RAM0_BEG 		= RAM_LIMIT_L + (128K * 3);
//RAM0_END 		= RAM_LIMIT_H;
//RAM0_SIZE 		= RAM0_END - RAM0_BEG;


RAM1_BEG 		= RAM_LIMIT_L;
RAM1_END 		= RAM0_BEG;
RAM1_SIZE 		= RAM1_END - RAM1_BEG;

CODE_BEG        = 0xFE00100;

MEMORY
{
	code0(rx)    	  : ORIGIN = CODE_BEG,  LENGTH = CONFIG_FLASH_SIZE
	ram0(rwx)         : ORIGIN = RAM0_BEG,  LENGTH = RAM0_SIZE
    dlog0(r)     	  : ORIGIN = 0x20000000, LENGTH = 0x100000
}


ENTRY(_start)

SECTIONS
{
    . = ORIGIN(ram0);
	.boot_info ALIGN(32):
	{
		*(.boot_info)
        . = ALIGN(128);
	} > ram0

	.irq_stack ALIGN(32):
    {
        _cpu0_sstack_begin = .;
        *(.cpu0_stack)
        _cpu0_sstack_end = .;
		. = ALIGN(4);
    } > ram0

	//TLB 起始需要4K 对齐；
    .mmu_tlb ALIGN(0x1000):
    {
        MMU_TBL_BEG_CHECK = .;
        *(.mmu_tlb_segment);
    } > ram0

	.data ALIGN(32):SUBALIGN(4)
	{
        . = ALIGN(4);
        *(.data_magic)

        *(.data*)
        *(.*.data)

         . = ALIGN(4);
         __a2dp_movable_slot_start = .;
         *(.movable.slot.1);
         __a2dp_movable_slot_end = .;


		. = ALIGN(4);
        app_mode_begin = .;
		KEEP(*(.app_mode))
        app_mode_end = .;

		. = ALIGN(4);
        prot_flash_begin = .;
        KEEP(*(.prot_flash))
        prot_flash_end = .;

		. = ALIGN(4);
        #include "btctrler/crypto/data.ld"

		. = ALIGN(4);

	} > ram0

	.bss ALIGN(32):SUBALIGN(4)
    {
        . = ALIGN(4);
        *(.bss)
        *(.*.data.bss)
        . = ALIGN(4);
        *(.*.data.bss.nv)

        . = ALIGN(4);
        *(.volatile_ram)

		*(.btstack_pool)

        . = ALIGN(4);
        *(.usb.data.bss.exchange)

        . = ALIGN(4);
		*(.non_volatile_ram)

        #include "btctrler/crypto/bss.ld"
        . = ALIGN(32);

    } > ram0

	.data_code ALIGN(32):SUBALIGN(4)
	{
		data_code_pc_limit_begin = .;
        *(.cache)
        *(.volatile_ram_code)

		. = ALIGN(4);
	} > ram0

    //for decompress
	//.data_code_compress ALIGN(32):SUBALIGN(4)
	.data_code_z ALIGN(32):SUBALIGN(4)
	{
		. = ALIGN(4);

        *(.os_critical_code)
        *(.os.text*)

        *(.chargebox_code)
        *(.movable.stub.1)
        *(.*.text.cache.L1)

        *(.ui_ram)
        *(.math_fast_funtion_code)


		/* *(.edr_link.text) */
		/* *(.lmp.text) */
		/* *(.baseband_manger.text) */
		/* *(.lmp_rxtx_v2.text) */
		/* *(.baseband.text) */
#if !(TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN))
		*(.tws_lmp_sync.text)
		*(.tws_conn_edr.text)
		*(.tws_esco.text)
		*(.jlstream.text.cache.L2)
#endif
		//*(.dac_node.text)
		//*(.jlstream.text)
		//*(.jlstream.text*)
		//*(.a2dp_streamctrl.text)
		//*(.a2dp_player.text)
		//*(.volume_node.text)
		//*(.bt_audio_timestamp.text)
		//*(.audio_src_coef.text)
		//*(.media.audio_libutils.text)
		//*(.dac_node.text)
		//*(.a2dp_streamctrl.text)

#if  TCFG_CODE_TO_RAM_COMPRESS_ENABLE
		*(.sbc_hwaccel.text)
		*(.eq_coeff_design.text)
		*(.aac.text)
		*(.media_analysis.text)
		*(.bt_audio_sync_node.text)
		*(.esco_audio_plc.text.cache.L2.run)
		*(.audio_cfifo.text.cache.L2.run)
		*(.node.eq.text.cache.L2)
		*(.node.source.text.cache.L2)
		*(.node.dac.text.cache.L2)
		*(.encoder_node.text)
		*(.decoder_node.text)
		*(.eq.text)
		*(.hw_eq.text)
		*(.audio_sync.text)
		*(.jlstream.text.cache.L2)
		*(.audio_anc_fade_ctr.text)
		*(.tone_player.text)
		*(.tws_tone_player.text)
		*(.media.media_cpu.text)
		*(.jlstream_fade.text)
		/* *(.anc.text) */
		/* *(.anc_core.text) */
		/* *(.anc_user.text) */
#endif

		. = ALIGN(4);
        _SPI_CODE_START = . ;
        *(.spi_code)
		. = ALIGN(4);
        _SPI_CODE_END = . ;

		. = ALIGN(4);
		#include "media/media_lib_data_text.ld"
		. = ALIGN(4);
	} > ram0

	__report_overlay_begin = .;
	overlay_code_begin = .;
    #include "app_overlay.ld"
#ifdef CONFIG_CODE_BANK_ENABLE
   //加个空段防止下面的OVERLAY地址不对
    .ram0_empty1 ALIGN(4) :
	{
        . = . + 4;
    } > ram0

    #include "bank/bank.ld"
    bank_code_run_addr = ADDR(.overlay_bank0);
#endif

	data_code_pc_limit_end = .;
	__report_overlay_end = .;

#if TCFG_CONFIG_DEBUG_RECORD_ENABLE
    // 需要避免与uboot和maskrom冲突
    .debug_record_ram ALIGN(4):
    {
        /* . = . + 0x1000; */
        . = ALIGN(4);
        *(.debug_record)
        . = ALIGN(4);
    } > ram0
#endif

	_HEAP_BEGIN = . ;
	_HEAP_END = RAM0_END;

    . = ORIGIN(code0);
    .text ALIGN(4):SUBALIGN(4)
    {
        PROVIDE(text_rodata_begin = .);

        *(.startup.text)

		*(.text)
		*(.*.text)
		*(.*.text.const)
        *(.*.text.const.cache.L2)
        *(.*.text.cache.L2*)
		. = ALIGN(4);

        bank_stub_start = .;
        *(.bank.stub.*)
        bank_stub_size = . - bank_stub_start;

	    update_target_begin = .;
	    PROVIDE(update_target_begin = .);
	    KEEP(*(.update_target))
	    update_target_end = .;
	    PROVIDE(update_target_end = .);
		. = ALIGN(4);

		*(.LOG_TAG_CONST*)
        *(.rodata*)

		. = ALIGN(4); // must at tail, make rom_code size align 4
        PROVIDE(text_rodata_end = .);

        clock_critical_handler_begin = .;
        KEEP(*(.clock_critical_txt))
        clock_critical_handler_end = .;

        chargestore_handler_begin = .;
        KEEP(*(.chargestore_callback_txt))
        chargestore_handler_end = .;

        . = ALIGN(4);
        app_msg_handler_begin = .;
		KEEP(*(.app_msg_handler))
        app_msg_handler_end = .;

        . = ALIGN(4);
        app_msg_prob_handler_begin = .;
		KEEP(*(.app_msg_prob_handler))
        app_msg_prob_handler_end = .;

		wireless_trans_ops_begin = .;
		KEEP(*(.wireless_trans))
		. = ALIGN(4);

		wireless_trans_ops_end = .;
		. = ALIGN(4);
		connected_sync_channel_begin = .;
		KEEP(*(.connected_call_sync))
		connected_sync_channel_end = .;
		. = ALIGN(4);
		conn_sync_call_func_begin = .;
		KEEP(*(.connected_sync_call_func))
		conn_sync_call_func_end = .;
		. = ALIGN(4);
		conn_data_trans_stub_begin = .;
		KEEP(*(.conn_data_trans_stub))
		conn_data_trans_stub_end = .;

        . = ALIGN(4);
        app_charge_handler_begin = .;
		KEEP(*(.app_charge_handler.0))
		KEEP(*(.app_charge_handler.1))
        app_charge_handler_end = .;

        . = ALIGN(4);
        scene_ability_begin = .;
		KEEP(*(.scene_ability))
        scene_ability_end = .;

         #include "media/framework/section_text.ld"

		. = ALIGN(4);
		tool_interface_begin = .;
		KEEP(*(.tool_interface))
		tool_interface_end = .;

        . = ALIGN(4);
        effects_online_adjust_begin = .;
        KEEP(*(.effects_online_adjust))
        effects_online_adjust_end = .;

        . = ALIGN(4);
        tws_tone_cb_begin = .;
        KEEP(*(.tws_tone_callback))
        tws_tone_cb_end = .;

        . = ALIGN(4);
        vm_reg_id_begin = .;
        KEEP(*(.vm_manage_id_text))
        vm_reg_id_end = .;

		/********maskrom arithmetic ****/
        *(.opcore_table_maskrom)
        *(.bfilt_table_maskroom)
        *(.bfilt_code)
        *(.bfilt_const)
		/********maskrom arithmetic end****/

		. = ALIGN(4);
		__VERSION_BEGIN = .;
        KEEP(*(.sys.version))
        __VERSION_END = .;

        *(.noop_version)
		. = ALIGN(4);

         __a2dp_text_cache_L2_start = .;
         *(.movable.region.1);
         . = ALIGN(4);
         __a2dp_text_cache_L2_end = .;
         . = ALIGN(4);
		/* #include "system/system_lib_text.ld" */
        #include "btctrler/crypto/text.ld"
        #include "btctrler/btctler_lib_text.ld"

        . = ALIGN(4);
		_record_handle_begin = .;
		PROVIDE(record_handle_begin = .);
		KEEP(*(.debug_record_handle_ops))
		_record_handle_end = .;
		PROVIDE(record_handle_end = .);

		. = ALIGN(32);
	  } > code0

    . = ORIGIN(dlog0);
    /* .dlog_data (NOLOAD):SUBALIGN(4) */
    .dlog_data :SUBALIGN(4)
    {
        dlog_seg_begin = .;
        // 头部的0x100空间
        KEEP(*(.dlog.rodata.head))
        /* . = 0x100 + ORIGIN(dlog0); */
		. = ALIGN(0x100);

        dlog_str_tab_seg_begin = .;
        dlog_str_tab_seg0_begin = .;
        *(.dlog.rodata.str_tab.0)

        dlog_str_tab_seg1_begin = .;
        *(.dlog.rodata.str_tab.1)

        dlog_str_tab_seg2_begin = .;
        *(.dlog.rodata.str_tab.2)

        dlog_str_tab_seg3_begin = .;
        *(.dlog.rodata.str_tab.3)

        dlog_str_tab_seg4_begin = .;
        *(.dlog.rodata.str_tab.4)

        dlog_str_tab_seg5_begin = .;
        *(.dlog.rodata.str_tab.5)

        dlog_str_tab_seg6_begin = .;
        *(.dlog.rodata.str_tab.6)

        dlog_str_tab_seg7_begin = .;
        *(.dlog.rodata.str_tab.7)

        dlog_str_tab_segAll_begin = .;
        *(.dlog.rodata.str_tab*)
        dlog_str_tab_segAll_end = .;
        dlog_str_tab_seg_end = .;

		. = ALIGN(32);
        dlog_str_seg_begin = .;
        dlog_str_seg0_begin = .;
        *(.dlog.rodata.string.0)

        dlog_str_seg1_begin = .;
        *(.dlog.rodata.string.1)

        dlog_str_seg2_begin = .;
        *(.dlog.rodata.string.2)

        dlog_str_seg3_begin = .;
        *(.dlog.rodata.string.3)

        dlog_str_seg4_begin = .;
        *(.dlog.rodata.string.4)

        dlog_str_seg5_begin = .;
        *(.dlog.rodata.string.5)

        dlog_str_seg6_begin = .;
        *(.dlog.rodata.string.6)

        dlog_str_seg7_begin = .;
        *(.dlog.rodata.string.7)

        dlog_str_segAll_begin = .;
        *(.dlog.rodata.string*)
        dlog_str_segAll_end = .;
        dlog_str_seg_end = .;

        dlog_seg_end = .;
    } > dlog0
}

#include "app.ld"
#include "update/update.ld"
#include "btstack/btstack_lib.ld"
//#include "btctrler/port/br28/btctler_lib.ld"
#include "driver/cpu/br56/driver_lib.ld"
#include "media/media_lib.c"
#include "utils/utils_lib.ld"
#include "cvp/audio_cvp_lib.ld"

#include "system/port/br56/system_lib.ld" //Note: 为保证各段对齐, 系统ld文件必须放在最后include位置

//================== Section Info Export ====================//
boot_info_addr = ADDR(.boot_info);
boot_info_size = SIZEOF(.boot_info);
ASSERT(boot_info_size >= 128,"!!! boot_info_size must larger than 128 Bytes !!!");

text_begin  = ADDR(.text);
text_size   = SIZEOF(.text);
text_end    = text_begin + text_size;
ASSERT((text_size % 4) == 0,"!!! text_size Not Align 4 Bytes !!!");

bss_begin = ADDR(.bss);
bss_size  = SIZEOF(.bss);
bss_end    = bss_begin + bss_size;
ASSERT((bss_size % 4) == 0,"!!! bss_size Not Align 4 Bytes !!!");

data_addr = ADDR(.data);
data_begin = text_begin + text_size;
data_size =  SIZEOF(.data);
ASSERT((data_size % 4) == 0,"!!! data_size Not Align 4 Bytes !!!");

data_code_addr = ADDR(.data_code);
data_code_begin = data_begin + data_size;
data_code_size =  SIZEOF(.data_code);
ASSERT((data_code_size % 4) == 0,"!!! data_code_size Not Align 4 Bytes !!!");

data_code_z_addr = ADDR(.data_code_z);
data_code_z_begin = data_code_begin + data_code_size;
data_code_z_size =  SIZEOF(.data_code_z);
ASSERT((data_code_z_size % 4) == 0,"!!! data_code_size Not Align 4 Bytes !!!");

bank_code_load_addr = data_code_z_begin;
common_code_run_addr = data_code_z_addr;

//================ OVERLAY Code Info Export ==================//
init_addr = ADDR(.overlay_init);
init_begin = data_code_z_begin + data_code_z_size;
init_size =  SIZEOF(.overlay_init);

aec_addr = ADDR(.overlay_aec);
aec_begin = init_begin + init_size;
aec_size =  SIZEOF(.overlay_aec);

aac_addr = ADDR(.overlay_aac);
aac_begin = aec_begin + aec_size;
aac_size =  SIZEOF(.overlay_aac);

//===================== HEAP Info Export =====================//

ASSERT(CONFIG_FLASH_SIZE > text_size,"check sdk_config.h CONFIG_FLASH_SIZE < text_size");
ASSERT(_HEAP_BEGIN > bss_begin,"_HEAP_BEGIN < bss_begin");
ASSERT(_HEAP_BEGIN > data_addr,"_HEAP_BEGIN < data_addr");
ASSERT(_HEAP_BEGIN > data_code_addr,"_HEAP_BEGIN < data_code_addr");
//ASSERT(_HEAP_BEGIN > __report_overlay_begin,"_HEAP_BEGIN < __report_overlay_begin");

PROVIDE(HEAP_BEGIN = _HEAP_BEGIN);
PROVIDE(HEAP_END = _HEAP_END);
_MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN;
PROVIDE(MALLOC_SIZE = _HEAP_END - _HEAP_BEGIN);

ASSERT(MALLOC_SIZE  >= 0x8000, "heap space too small !")
//ASSERT(MMU_TBL_BEG_CHECK == 0x102000, "MMU table begin MUST at 0x102000") //by Gw

//===================== dlog check =====================//
// 宏DLOG_STR_TAB_STRUCT_SIZE = 16 (即 dlog_str_tab_s 结构体的大小), 0xFFFF是支持的最大log index数
STR_TAB_SIZE = 16;
ASSERT((dlog_str_tab_seg_end - dlog_str_tab_seg_begin) <= (STR_TAB_SIZE * 0xFFFF), "err: log index out of range, only 0x0000 ~ 0xFFFF !!!");
ASSERT((dlog_str_tab_seg_begin - ADDR(.dlog_data)) <= 0x100, "err: .dlog.rodata.head out of range, only less than 0x100 !!!");
// 定义异常信息和dlog打印数据在保存dlog数据区域的起始地址
#if TCFG_CONFIG_DEBUG_RECORD_ENABLE
dlog_exception_data_start_addr = 0x0FFF;   // 这个地址是相对于保存dlog数据起始地址的偏移
dlog_log_data_start_addr = 0x1000;         // 这个地址是相对于保存dlog数据起始地址的偏移
#else
dlog_exception_data_start_addr = 0;
dlog_log_data_start_addr = 0;
#endif

//============================================================//
//=== report section info begin:
//============================================================//
report_text_beign = ADDR(.text);
report_text_size = SIZEOF(.text);
report_text_end = report_text_beign + report_text_size;

report_mmu_tlb_begin = ADDR(.mmu_tlb);
report_mmu_tlb_size = SIZEOF(.mmu_tlb);
report_mmu_tlb_end = report_mmu_tlb_begin + report_mmu_tlb_size;

report_boot_info_begin = ADDR(.boot_info);
report_boot_info_size = SIZEOF(.boot_info);
report_boot_info_end = report_boot_info_begin + report_boot_info_size;

report_irq_stack_begin = ADDR(.irq_stack);
report_irq_stack_size = SIZEOF(.irq_stack);
report_irq_stack_end = report_irq_stack_begin + report_irq_stack_size;

report_data_begin = ADDR(.data);
report_data_size = SIZEOF(.data);
report_data_end = report_data_begin + report_data_size;

report_bss_begin = ADDR(.bss);
report_bss_size = SIZEOF(.bss);
report_bss_end = report_bss_begin + report_bss_size;

report_data_code_begin = ADDR(.data_code);
report_data_code_size = SIZEOF(.data_code);
report_data_code_end = report_data_code_begin + report_data_code_size;

report_overlay_begin = __report_overlay_begin;
report_overlay_size = __report_overlay_end - __report_overlay_begin;
report_overlay_end = __report_overlay_end;

report_heap_beign = _HEAP_BEGIN;
report_heap_size = _HEAP_END - _HEAP_BEGIN;
report_heap_end = _HEAP_END;


br56_PHY_RAM_SIZE = PHY_RAM_SIZE;
br56_SDK_RAM_SIZE = report_mmu_tlb_size + \
					report_boot_info_size + \
					report_irq_stack_size + \
					report_data_size + \
					report_bss_size + \
					report_overlay_size + \
					report_data_code_size + \
					report_heap_size;
//============================================================//
//=== report section info end
//============================================================//


