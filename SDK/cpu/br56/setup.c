#include "cpu/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "audio_config_def.h"
#include "generic/lbuf.h"

#define LOG_TAG             "[SETUP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

extern void sys_timer_init(void);
extern void tick_timer_init(void);
extern void exception_irq_handler(void);
extern int __crc16_mutex_init();
extern void debug_uart_init();

/* --------------------------------------------------------------------------*/
/**
 * @brief 裸机程序入口
 */
/* ----------------------------------------------------------------------------*/
void system_main(void)
{
    //分支预测
    q32DSP(core_num())->PMU_CON1 &= ~BIT(8);

    wdt_close();

#if (defined TCFG_FIX_CLOCK_FREQ  && ((TCFG_FIX_CLOCK_FREQ == 240000000) || (TCFG_FIX_CLOCK_FREQ == 160000000)))|| TCFG_IIS_NODE_ENABLE
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 240 * MHz);
#else
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 192 * MHz);
#endif
    //heap内存管理，STDIO需要heap
    memory_init();

    //uart init
    debug_uart_init();

    //STDIO init
    log_early_init(1024 * 2);

    //debug info

    while (1) {
        asm("idle");
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 裸机环境启动，进入无操作系统环境
 */
/* ----------------------------------------------------------------------------*/
#define SYSTEM_SSP_SIZE 512
u32 system_ssp_stack[SYSTEM_SSP_SIZE];//2Kbyte
void system_start(void)
{
    asm("ssp = %0"::"i"(system_ssp_stack + SYSTEM_SSP_SIZE));
    asm("r0 = %0"::"i"(system_main));
    asm("reti = r0");
    asm("rti");
}

#if 0
___interrupt
void exception_irq_handler(void)
{
    ___trig;

    exception_analyze();

    log_flush();
    while (1);
}
#endif



/*
 * 此函数在cpu0上电后首先被调用,负责初始化cpu内部模块
 *
 * 此函数返回后，操作系统才开始初始化并运行
 *
 */

#if 0
static void early_putchar(char a)
{
    if (a == '\n') {
        UT2_BUF = '\r';
        __asm_csync();
        while ((UT2_CON & BIT(15)) == 0);
    }
    UT2_BUF = a;
    __asm_csync();
    while ((UT2_CON & BIT(15)) == 0);
}

void early_puts(char *s)
{
    do {
        early_putchar(*s);
    } while (*(++s));
}
#endif

void cpu_assert_debug()
{
#if CONFIG_DEBUG_ENABLE
    log_flush();
    local_irq_disable();
    while (1);
#else
    system_reset(ASSERT_FLAG);
#endif
}

_NOINLINE_
void cpu_assert(char *file, int line, bool condition, char *cond_str)
{
    if (config_asser) {
        if (!(condition)) {
            printf("cpu %d file:%s, line:%d\n", current_cpu_id(), file, line);
            printf("ASSERT-FAILD: %s\n", cond_str);
            cpu_assert_debug();
        }
    } else {
        if (!(condition)) {
            system_reset(ASSERT_FLAG);
        }
    }
}

__attribute__((weak))
void maskrom_init(void)
{
    return;
}

void app_bank_init()
{
#ifdef CONFIG_CODE_BANK_ENABLE
    request_irq(IRQ_SYSCALL_IDX, 7, bank_syscall_entry, 0);
#endif
}

//==================================================//
extern u32 icache0_ram_data_code_addr;
extern u32 icache0_ram_data_code_begin;
extern u32 icache0_ram_data_code_size;

struct lbuff_head *icache0_lbuf = NULL;

#if TCFG_FREE_ICACHE0_WAY_NUM
u8 icache0_buf[TCFG_FREE_ICACHE0_WAY_NUM * 8 * 1024] SEC(.icache0_pool);
void *get_icache0_lbuf(void)
{
    return icache0_lbuf;
}
#endif


__attribute__((weak))
AT(.volatile_ram_code)
void cache_ram_init(void)
{
#if TCFG_FREE_ICACHE0_WAY_NUM
    // 用 volatile 避免被优化而导致出错
    volatile u8 icache0_way = 4 - TCFG_FREE_ICACHE0_WAY_NUM;
    if (icache0_way != 4) {
        IcuDisable();
        IcuInitial();
        // 空出way当ram用
        IcuSetWayNum(icache0_way);
        memcpy((u8 *)&icache0_ram_data_code_addr, (u8 *)&icache0_ram_data_code_begin, (u32)&icache0_ram_data_code_size);

        icache0_lbuf = lbuf_init(icache0_buf, sizeof(icache0_buf), 4, 0);
    }
#endif
}

#if TCFG_NORMAL_SET_DUT_MODE
AT(.volatile_ram_code)
void debug_random_read(u32 size)
{
    u32 ali_start = rand32();
    u32 ali_end;
    volatile u8 tmp;
    extern u32 CODE_BEG;
    ali_start %= 32 * 1024;
    ali_start += (u32)&CODE_BEG;
    ali_end = ali_start + size;

    for (u32 i = ali_start;  i <  ali_end; i += 32) {
        tmp = *(u8 *)i;
    }

    /* local_irq_disable(); */
    /* extern void sfc_drop_cache(void *ptr, u32 len); */
    /* sfc_drop_cache((void *)ali_start, size); */
    /* local_irq_enable(); */
}

void idle_hook(void)
{
    while (1) {
        wdt_close();
        debug_random_read(128 * 1024);
    }
}
#endif

/* extern void gpio_longpress_pin0_reset_config(u32 pin, u32 level, u32 time); */
void memory_init(void);

__attribute__((weak))
void app_main()
{
    while (1) {
        asm("idle");
    }
}
#define     IIO_DEBUG_TOGGLE(i,x)  {JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT ^= BIT(x);}
void io_toggle()
{
    IIO_DEBUG_TOGGLE(A, 3);
    IIO_DEBUG_TOGGLE(A, 3);
}

void setup_arch()
{
    q32DSP(core_num())->PMU_CON1 &= ~BIT(8); //分支预测

#if TCFG_LONG_PRESS_RESET_ENABLE
    gpio_longpress_pin0_reset_config(TCFG_LONG_PRESS_RESET_PORT, TCFG_LONG_PRESS_RESET_LEVEL, TCFG_LONG_PRESS_RESET_TIME, 1, TCFG_LONG_PRESS_RESET_INSIDE_PULL_UP_DOWN, 0);
#else
    gpio_longpress_pin0_reset_config(IO_PORTB_01, 0, 0, 1, 1, 0);
#endif

    memory_init();

    wdt_init(WDT_16S);
    /* wdt_close(); */

    gpadc_mem_init(8);
    efuse_init();

    sdfile_init();
    syscfg_tools_init();

#if (TCFG_DAC_POWER_MODE == 0)      // power_mode: 20mW
    clk_set_vdc_lowest_voltage(DCVDD_VOL_115V);
#elif (TCFG_DAC_POWER_MODE == 1)    // power_mode: 30mW
    clk_set_vdc_lowest_voltage(DCVDD_VOL_130V);
#elif (TCFG_DAC_POWER_MODE == 2)    // power_mode: 50mW
#if ((defined TCFG_AUDIO_DAC_CLASSH_EN) && (TCFG_AUDIO_DAC_CLASSH_EN == 1))
    clk_set_dcvdd_audio_ctl(DCVDD_VOL_155V, DCVDD_VOL_115V);
#else
    clk_set_vdc_lowest_voltage(DCVDD_VOL_155V);
#endif
#endif

#if (TCFG_MAX_LIMIT_SYS_CLOCK==MAX_LIMIT_SYS_CLOCK_160M)
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 240 * MHz);//  240:max clock 160
#else
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 192 * MHz);
#endif
    os_init();
    tick_timer_init();
    sys_timer_init();

#if CONFIG_DEBUG_ENABLE || CONFIG_DEBUG_LITE_ENABLE
    debug_uart_init();
#if CONFIG_DEBUG_ENABLE
    log_early_init(1024 * 2);
#endif
#endif

    log_i("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    log_i("         setup_arch");
    log_i("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

    power_early_flowing();

    clock_dump();

    void mvbg_current_trim();        //pmu used
    mvbg_current_trim();
    void audio_vbg_current_trim();        //audio used
    audio_vbg_current_trim();

    //Register debugger interrupt
    request_irq(0, 2, exception_irq_handler, 0);
    request_irq(1, 2, exception_irq_handler, 0);
#ifdef CONFIG_CODE_MOVABLE_ENABLE
    code_movable_init();
#endif

    debug_init();

    __crc16_mutex_init();

    app_main();
}

void exception_analyze_user_callback(int unsigned *sp)
{
    // debug something after exception
    printf("exception_analyze_user_callback:\n");
    extern void mem_unfree_dump();
    mem_unfree_dump();
}


