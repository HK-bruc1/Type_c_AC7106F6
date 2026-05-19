#include "app_config.h"
#include "cpu/includes.h"
#include "system/includes.h"
#include "led_spi.h"

#if 1

#define LOG_TAG_CONST       LED
#define LOG_TAG             "[LED]"
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"
#else

#define log_debug   printf

#endif


#define LED_SPI_DAT_BAUD        8000000
#define LED_SPI_REST_BAUD       1000000

static OS_SEM led_spi_sem;
static hw_spi_dev led_spi_dev;
static u8 led_spi_busy = 0;
static u8 led_spi_sus = 0;


void led_spi_isr_cbfunc(hw_spi_dev spi, enum hw_spi_isr_status sta)
{
    spi_set_ie(led_spi_dev, 0);
    os_sem_post(&led_spi_sem);
    led_spi_busy = 0;
}

void led_spi_init(hw_spi_dev spi, u32 spi_do)
{
    os_sem_create(&led_spi_sem, 1);

    spi_hardware_info led_spi_pdata = {
        .port[0] = -1,
        .port[1] = spi_do,
        .port[2] = -1,
        .port[3] = -1,
        .port[4] = -1,
        .port[5] = -1,
        .role = SPI_ROLE_MASTER,
        .mode = SPI_MODE_UNIDIR_1BIT,
        .bit_mode = SPI_FIRST_BIT_MSB,
        .cpol = 0,
        .cpha = 0,
        .ie_en = 1,
        .irq_priority = 3,
        .spi_isr_callback = led_spi_isr_cbfunc,
        .clk = LED_SPI_DAT_BAUD,
    };
    led_spi_dev = spi;
    spi_open(led_spi_dev, &led_spi_pdata);
    spi_set_ie(led_spi_dev, 0);
    spi_send_byte(led_spi_dev, 0);
}

void led_spi_rgb_to_24byte(u8 r, u8 g, u8 b, u8 *buf, int idx)
{
    buf = buf + idx * 24;
    u32 dat = ((g << 16) | (r << 8) | b);
    for (u8 i = 0; i < 24; i ++) {
        if (dat & BIT(23 - i)) {
            *(buf + i) = 0x7c;
        } else {
            *(buf + i) = 0x60;
        }
    }
}

void led_spi_rgb_to_24byte2(u32 data, u8 *buf)
{
    u32 dat = data;
    for (u8 i = 0; i < 24; i ++) {
        ///要抓波形，发完一byte数据有个结束波形（如果开机是低电平，结束波形就是高，反之为低，时间为几十ns）
        ///根据结束波形的高低来确认BIT(7)的高低，要一样，因为结束后接着发下一byte的BIT(7)
        if (dat & BIT(23 - i)) {
            *(buf + i) = 0x7c;
        } else {
            *(buf + i) = 0x60;
        }
    }
}

void led_spi_send_reset()
{
    u8 tmp_buf[16] = {0};
    spi_set_baud(led_spi_dev, LED_SPI_REST_BAUD);
    spi_dma_send(led_spi_dev, (const void *)tmp_buf, 16);
}

void led_spi_send_rgbbuf(u8 *rgb_buf, u16 led_num) //rgb_buf的大小 至少要等于 led_num * 24
{
    if (!led_num) {
        return;
    }
    led_spi_busy = 1;
    led_spi_send_reset();
    spi_set_baud(led_spi_dev, LED_SPI_DAT_BAUD);
    spi_dma_send(led_spi_dev, (const void *)rgb_buf, led_num * 24);
    led_spi_busy = 0;
}

void led_spi_send_rgbbuf_isr(u8 *rgb_buf, u16 led_num) //rgb_buf的大小 至少要等于 led_num * 24
{
    if (!led_num) {
        return;
    }
    led_spi_busy = 1;
    os_sem_pend(&led_spi_sem, 0);
    led_spi_send_reset();
    spi_set_baud(led_spi_dev, LED_SPI_DAT_BAUD);
    spi_dma_transmit_for_isr(led_spi_dev, rgb_buf, led_num * 24, 0);
    spi_set_ie(led_spi_dev, 1);
}

u32 led_spi_suspend(void)
{
    if (led_spi_sus) {
        return 1;
    }
    if (led_spi_busy) {
        return 1;
    }
    spi_suspend(led_spi_dev);
    led_spi_sus = 1;
    return 0;
}

u32 led_spi_resume(void)
{
    if (!led_spi_sus) {
        return 0;
    }
    spi_resume(led_spi_dev);
    led_spi_sus = 0;
    return 0;
}

static u8 spi_dat_buf[24 * 2] __attribute__((aligned(4)));

void led_spi_test_task(void *p)
{
    static u8 cnt = 0;

    cnt ++;

    led_spi_rgb_to_24byte(cnt, 255 - cnt, 0, spi_dat_buf, 0);
    led_spi_rgb_to_24byte(0, 0, cnt, spi_dat_buf, 1);

#if 0
    led_spi_send_rgbbuf(spi_dat_buf, 2);        //等待的方式，建议用在发的数据量小的场合
#else
    led_spi_send_rgbbuf_isr(spi_dat_buf, 2);    //中断的方式，建议用在发的数据量大的场合
#endif
}


void led_spi_test(void)
{
    printf("******************  led spi test  *******************\n");

    led_spi_init(HW_SPI1, IO_PORTC_03);

    sys_timer_add(NULL, led_spi_test_task, 20);
}


