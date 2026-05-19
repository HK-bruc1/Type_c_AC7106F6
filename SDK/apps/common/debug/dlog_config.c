#include "typedef.h"
#include "app_config.h"
#include "fs/sdfile.h"
#include "fs/resfile.h"
#include "boot.h"
#include "asm/sfc_norflash_api.h"
#include "norflash.h"
#include "system/includes.h"

#if TCFG_DEBUG_DLOG_ENABLE
extern u32 __attribute__((weak)) dlog_log_data_start_addr[];

__attribute__((weak))
int dlog_output_direct(void *buf, u16 len)
{
    return 0;
}

#if 0
void dlog_print_test1(void *priv)
{
    static int cnt[3] = {0};
    char time[15] = {0};
    int index = (int)priv;

    extern int log_print_time_to_buf(char *time);
    log_print_time_to_buf(time);

    /* printf("%stask:%s, cnt:%d\n", time, os_current_task(), cnt[index]); */
    dlog_printf("%stask:%s, cnt:%d\n", time, os_current_task(), cnt[index]);
    /* dlog_printf("%stask:%s, %s, %s, %s, %s, cnt:%d\n", time, os_current_task(), __func__, __FILE__, __FILE__, __FILE__, cnt[index]); */
    /* printf("%stask:%s, %s, %s, %s, %s, cnt:%d\n", time, os_current_task(), __func__, __FILE__, __FILE__, __FILE__, cnt[index]); */
    cnt[index]++;
}

void dlog_print_in_bttask()
{
    printf("%s, %d", __FUNCTION__, __LINE__);
    sys_timer_add((void *)1, dlog_print_test1, 188);
}

void dlog_print_in_btctrler()
{
    printf("%s, %d", __FUNCTION__, __LINE__);
    sys_timer_add((void *)2, dlog_print_test1, 115);
}

void dlog_print_test(void *priv)
{
    extern void dlog_uart_init();
    dlog_uart_init();
    sys_timer_add((void *)0, dlog_print_test1, 200);
#if 1
    int msg[] = {(int)dlog_print_in_bttask, 0};
    os_taskq_post_type("btstack", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    int msg2[] = {(int)dlog_print_in_btctrler, 0};
    os_taskq_post_type("btctrler", Q_CALLBACK, ARRAY_SIZE(msg2), msg2);
#endif
}
#endif

#if TCFG_DEBUG_DLOG_FLASH_SEL // 外置flash获取其它自定义的区域

extern int _norflash_init(const char *name, struct norflash_dev_platform_data *pdata);
extern int _norflash_open(void *arg);
extern int _norflash_read(u32 addr, u8 *buf, u32 len, u8 cache);
extern int _norflash_write(u32 addr, void *buf, u32 len, u8 cache);
extern int _norflash_eraser(u8 eraser, u32 addr);
extern int _norflash_close(void);
extern int _norflash_ioctl(u32 cmd, u32 arg, u32 unit, void *_part);

u8 dlog_use_ex_flash = 0;

//重写弱函数
#ifdef CONFIG_CPU_BR36
const struct spi_platform_data spix_p_data[HW_SPI_MAX_NUM] = {
    {
        //spi0
    },
    {
        //spi1
        .port = {
            IO_PORTA_07, //clk any io
            IO_PORTC_00, //do any io
            IO_PORTB_05, //di any io
            0xff, //d2 any io
            0xff, //d3 any io
            0xff, //cs any io(主机不操作cs)
        },
        .role = 0,//SPI_ROLE_MASTER,
        .clk  = 1000000,
        .mode = 0,//SPI_MODE_BIDIR_1BIT,//SPI_MODE_UNIDIR_2BIT,
        .bit_mode = 0, //SPI_FIRST_BIT_MSB
        .cpol = 0,//clk level in idle state:0:low,  1:high
        .cpha = 0,//sampling edge:0:first,  1:second
        .ie_en = 0, //ie enbale:0:disable,  1:enable
        .irq_priority = 3,
        .spi_isr_callback = NULL,  //spi isr callback
    },
    {
        //spi2
    },
};

static const struct norflash_dev_platform_data spi_flash_platform_data = {
    .spi_hw_num     = 1,
    .spi_cs_port    = IO_PORTA_04,
    .spi_read_width = 0,
    .start_addr     = TCFG_NORFLASH_START_ADDR,
    .size           = TCFG_NORFLASH_SIZE,
};
#endif

//重写弱函数
#ifdef CONFIG_CPU_BR56
const struct spi_platform_data spix_p_data[HW_SPI_MAX_NUM] = {
    {
        //spi0
    },
    {
        //spi1
        .port = {
            IO_PORTA_04, //clk any io
            IO_PORTA_05, //do any io
            IO_PORTC_00, //di any io
            0xff, //d2 any io
            0xff, //d3 any io
            0xff, //cs any io(主机不操作cs)
        },
        .role = 0,//SPI_ROLE_MASTER,
        .clk  = 1000000,
        .mode = 0,//SPI_MODE_BIDIR_1BIT,//SPI_MODE_UNIDIR_2BIT,
        .bit_mode = 0, //SPI_FIRST_BIT_MSB
        .cpol = 0,//clk level in idle state:0:low,  1:high
        .cpha = 0,//sampling edge:0:first,  1:second
        .ie_en = 0, //ie enbale:0:disable,  1:enable
        .irq_priority = 3,
        .spi_isr_callback = NULL,  //spi isr callback
    },
};

static const struct norflash_dev_platform_data spi_flash_platform_data = {
    .spi_hw_num     = 1,
    .spi_cs_port    = IO_PORTA_06,
    .spi_read_width = 0,
    .start_addr     = TCFG_NORFLASH_START_ADDR,
    .size           = TCFG_NORFLASH_SIZE,
};
#endif


#ifdef CONFIG_CPU_BR52
const struct spi_platform_data spix_p_data[HW_SPI_MAX_NUM] = {
    {
        //spi0
    },
    {
        //spi1
        .port = {
            IO_PORTC_06, //clk any io
            IO_PORTC_07, //do any io
            IO_PORTC_00, //di any io
            0xff, //d2 any io
            0xff, //d3 any io
            0xff, //cs any io(主机不操作cs)
        },
        .role = 0,//SPI_ROLE_MASTER,
        .clk  = 1000000,
        .mode = 0,//SPI_MODE_BIDIR_1BIT,//SPI_MODE_UNIDIR_2BIT,
        .bit_mode = 0, //SPI_FIRST_BIT_MSB
        .cpol = 0,//clk level in idle state:0:low,  1:high
        .cpha = 0,//sampling edge:0:first,  1:second
        .ie_en = 0, //ie enbale:0:disable,  1:enable
        .irq_priority = 3,
        .spi_isr_callback = NULL,  //spi isr callback
    },
    {
        //spi2
    },
};

static const struct norflash_dev_platform_data spi_flash_platform_data = {
    .spi_hw_num     = 1,
    .spi_cs_port    = IO_PORTC_05,
    .spi_read_width = 0,
    .start_addr     = TCFG_NORFLASH_START_ADDR,
    .size           = TCFG_NORFLASH_SIZE,
};
#endif

/*----------------------------------------------------------------------------*/
/**@brief 获取保存dlog数据的flash起始地址和大小
  @param  addr: 返回起始地址
  @param  len: 返回长度
  @param  offset: 数据需要写入的flash地址
  @return 等于0表示成功; 小于0表示失败
  @note   起始地址和长度必需 4K 对齐
 */
/*----------------------------------------------------------------------------*/
static int dlog_get_ex_flash_zone(u32 *addr, u32 *len)
{
    // 需要实现
    if (addr) {
        *addr = TCFG_DLOG_FLASH_START_ADDR + (u32)dlog_log_data_start_addr;
    }
    if (len) {
        *len = TCFG_DLOG_FLASH_REGION_SIZE - (u32)dlog_log_data_start_addr;
    }

    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief 将flash的指定扇区擦除
  @param  erase_sector: 要擦除的扇区偏移(偏移是相对于保存dlog数据的flash区域起始地址)
  @param  sector_num: 要擦除的扇区个数
  @param  offset: 数据需要写入的flash地址
  @return 等于0表示成功; 小于0表示失败
  @note
 */
/*----------------------------------------------------------------------------*/
static int dlog_ex_flash_zone_erase(u16 erase_sector, u16 sector_num)
{
    // 需要实现
    /* printf("%s, %d, %d", __func__, erase_sector, sector_num); */

    if (dlog_use_ex_flash) {
        for (int i = 0; i < sector_num; i++) {
            _norflash_eraser(FLASH_SECTOR_ERASER, (erase_sector + i) * LOG_BASE_UNIT_SIZE + TCFG_DLOG_FLASH_START_ADDR + (u32)dlog_log_data_start_addr);
        }
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief 将dlog数据写入flash
  @param  buf: 要写入的数据
  @param  len: 要写入的数据长度
  @param  offset: 数据需要写入的flash地址
  @return 返回写入的长度,返回值小于0表示写入失败
  @note
 */
/*----------------------------------------------------------------------------*/
static int dlog_ex_flash_write(void *buf, u16 len, u32 offset)
{
    // 需要实现
    if (!len) {
        return 0;
    }

    if (dlog_use_ex_flash) {
        _norflash_write(offset, buf, len, 0);
    }

    /* printf("%s, %d", __func__, len); */
    /* put_buf(buf, len); */
    return len;
}

/*----------------------------------------------------------------------------*/
/**@brief 从flash中读dlog数据
  @param  buf: 返回要读取的数据
  @param  len: 需要读取的数据长度
  @param  offset: 需要读取的flash地址
  @return 返回写入的长度,返回值小于0表示写入失败
  @note
 */
/*----------------------------------------------------------------------------*/
static int dlog_ex_flash_read(void *buf, u16 len, u32 offset)
{
    // 需要实现
    if (dlog_use_ex_flash) {
        _norflash_read(offset, buf, len, 0);
    }
    /* printf("%s, %d", __func__, len); */
    /* put_buf(buf, len); */
    return len;
}

// 返回 0 表示初始化成功
static int dlog_ex_flash_init(void)
{
    int err;

#ifdef CONFIG_CPU_BR36
    gpio_set_mode(IO_PORT_SPILT(IO_PORTC_01), PORT_OUTPUT_HIGH);
#endif
#ifdef CONFIG_CPU_BR52
    gpio_set_mode(IO_PORT_SPILT(IO_PORTB_04), PORT_OUTPUT_HIGH);
#endif
#ifdef CONFIG_CPU_BR56
    gpio_set_mode(IO_PORT_SPILT(IO_PORTA_03), PORT_OUTPUT_HIGH);
#endif
    os_time_dly(1);
    _norflash_init("flash1", (struct norflash_dev_platform_data *)&spi_flash_platform_data);

    err = _norflash_open(NULL);
    if (err) {
        _norflash_close();
        gpio_set_mode(IO_PORT_SPILT(IO_PORTC_01), PORT_HIGHZ);
    } else {
        dlog_use_ex_flash = 1;
    }
    //sys_timeout_add(NULL, dlog_print_test, 3000);
    return 0;
}

REGISTER_DLOG_OPS(ex_flash_op, 1) = {
    .dlog_output_init       = dlog_ex_flash_init,
    .dlog_output_get_zone   = dlog_get_ex_flash_zone,
    .dlog_output_zone_erase = dlog_ex_flash_zone_erase,
    .dlog_output_write      = dlog_ex_flash_write,
    .dlog_output_read       = dlog_ex_flash_read,
    .dlog_output_direct     = dlog_output_direct,
};

#else  // 内置flash


#define DLOG_FILE_PATH  		          "flash/app/dlog"

struct inside_flash_info_s {
    u32 addr;
    u32 len;
};

static struct inside_flash_info_s flash_info;

static int dlog_get_inside_flash_zone(u32 *addr, u32 *len)
{
    if (addr) {
        *addr = flash_info.addr;
    }
    if (len) {
        *len = flash_info.len;
    }

    return 0;
}

static int dlog_inside_flash_zone_erase(u16 erase_sector, u16 sector_num)
{
    int ret = -1;

    if ((flash_info.addr == 0) || (flash_info.len == 0)) {
        return ret;
    }

    u32 erase_addr = sdfile_cpu_addr2flash_addr(flash_info.addr + erase_sector * LOG_BASE_UNIT_SIZE);
    u32 erase_unit = LOG_BASE_UNIT_SIZE;
    u32 erase_size = 0;
    u32 total_size = LOG_BASE_UNIT_SIZE * sector_num;
    u32 erase_cmd = IOCTL_ERASE_SECTOR;
    if (boot_info.vm.align == 1) {
        erase_unit = 256;
        erase_cmd = IOCTL_ERASE_PAGE;
    }
    do {
        ret = norflash_ioctl(NULL, erase_cmd, erase_addr);
        if (ret < 0) {
            break;
        }
        erase_addr += erase_unit;
        erase_size += erase_unit;
    } while (erase_size < total_size);

    return ret;
}

static int dlog_inside_flash_init(void)
{
    RESFILE *fp = resfile_open(DLOG_FILE_PATH);
    if (fp == NULL) {
        printf("Open %s dlog file fail", DLOG_FILE_PATH);
        return -1;
    }

    struct resfile_attrs file_attr = {0};
    resfile_get_attrs(fp, &file_attr);
    flash_info.addr = file_attr.sclust;
    flash_info.len = file_attr.fsize;

    if (fp) {
        resfile_close(fp);
    }

    printf("dlog flash init ok!\n");
    printf("dlog file addr 0x%x, len %d\n", flash_info.addr, flash_info.len);

    return 0;
}

static int dlog_inside_flash_write(void *buf, u16 len, u32 offset)
{
    int ret = -1;

    if ((flash_info.addr == 0) || (flash_info.len == 0)) {
        return ret;
    }

    u32 write_addr = sdfile_cpu_addr2flash_addr(offset); //flash addr
    printf("flash write addr 0x%x\n", write_addr);
    //写flash:
    ret = norflash_write(NULL, (void *)buf, len, write_addr);
    if (ret != len) {
        printf("dlog data save to flash err\n");
    }

    return ret;
}

static int dlog_inside_flash_read(void *buf, u16 len, u32 offset)
{
    memcpy(buf, (u8 *)offset, len);

    return len;
}

REGISTER_DLOG_OPS(ex_flash_op, 0) = {
    .dlog_output_init       = dlog_inside_flash_init,
    .dlog_output_get_zone   = dlog_get_inside_flash_zone,
    .dlog_output_zone_erase = dlog_inside_flash_zone_erase,
    .dlog_output_write      = dlog_inside_flash_write,
    .dlog_output_read       = dlog_inside_flash_read,
    .dlog_output_direct     = dlog_output_direct,
};

#endif


u16 dlog_read_log_data(u8 *buf, u16 len, u32 offset)
{
    u16 ret = -1;
    if ((offset + len) <= (u32)dlog_log_data_start_addr) {
        memset(buf, 0xFF, len);
        return len;
    } else {
        int remain_len = (int)((u32)dlog_log_data_start_addr - offset);
        remain_len = remain_len > 0 ? remain_len : 0;
        if (remain_len) {
            memset(buf, 0xFF, remain_len);
        }
        ret = dlog_read_from_flash(buf + remain_len, len - remain_len, (offset + remain_len) - (u32)dlog_log_data_start_addr);
        if ((len - remain_len) == ret) {
            return len;
        }
    }

    return ret;
}

u8 dlog_get_flash_exsit(void)
{
    u32 flash_id = 0;
    _norflash_ioctl(IOCTL_GET_ID, (u32)&flash_id, 0, NULL);
    if ((flash_id == 0) || (flash_id == 0xffffff))  {
        return 0;
    }

    return 1;
}



#if 0  // dlog demo
// 以下是部分离线log接口的使用示例

void dlog_demo(void)
{
    int ret;
    // 1,刷新将离线log数据从ram保存到flash
    // 仅仅只发送一个刷新消息后退出, 可以用于中断和任务
    ret = dlog_flush2flash(0);
    // 一直等待刷新成功, 若返回成功, 从函数返回后已经刷新到flash, 仅可用于任务
    ret = dlog_flush2flash(-1);
    // 等待1000ms, 若返回成功, 从函数返回后已经刷新到flash, 仅可用于任务
    ret = dlog_flush2flash(100);


    // 2,设置log的等级, 主要用于 log_xxx 类接口
    // 设置为 info 等级, 更多等级见 debug.h
    dlog_level_set(LOG_INFO);


    // 3,设置log输出的方式
    // 设置为仅串口输出离线log
    if (dlog_output_type_get() & DLOG_OUTPUT_2_FLASH) {
        // 如果之前有使能flash输出, 那么关闭flash输出前需要刷新ram的log到flash
        ret = dlog_flush2flash(100);
    }
    dlog_output_type_set(DLOG_OUTPUT_2_UART);
    // 离线log输出加上flash输出(如原本是仅输出到串口,设置后log同时输出到串口和flash)
    dlog_output_type_set(dlog_output_type_get() | DLOG_OUTPUT_2_FLASH);


    // 4,离线log的时间、日期、序号进行同步(当收到手机下发的日期、时间后需要同步一次, 以校准离线log时间)
    dlog_info_sync();


    // 5,读取离线log数据
#define TMP_BUF_SIZE         501
    u8 *tmp_buf = malloc(TMP_BUF_SIZE);
    u32 offset = 0;
    int ret;
    u8 flash_log_en = (dlog_output_type_get() & DLOG_OUTPUT_2_FLASH);  // 获取是否有使能log输出到flash
    if (flash_log_en) {
        dlog_output_type_set(dlog_output_type_get() & (~DLOG_OUTPUT_2_FLASH));  // 禁止log输出到flash
    }
    while (1) {
        // 建议使用dlog_read_log_data接口, 而不是dlog_read_from_flash接口
        ret = dlog_read_log_data(tmp_buf, TMP_BUF_SIZE, offset);  // 读取flash中的log, 返回0表示已读取完flash的全部log
        if (0 == ret) {
            // 已经全部读取
            break;
        }
        offset += ret;
        put_buf(tmp_buf, ret > 16 ? 16 : ret);  // 将读取的log数据前16Byte打印出来
    }
    if (flash_log_en) {
        dlog_output_type_set(dlog_output_type_get() | DLOG_OUTPUT_2_FLASH);  // 恢复log输出到flash
    }
    free(tmp_buf);
}

// 重写弱函数的实现示例
int dlog_get_rtc_time(void *time_p)
{
    // 仅需返回年月日
    struct sys_time *time = time_p;
    u32 year  = get_sys_year();  // get_sys_year函数需要自行实现,此处仅示例
    u32 month = get_sys_month(); // get_sys_month函数需要自行实现,此处仅示例
    u32 day   = get_sys_day();   // get_sys_day函数需要自行实现,此处仅示例
    time->year = year;
    time->month = month;
    time->day = day;

    return 0;  // 返回大于等于0表示成功
}

// 重写弱函数的实现示例
u32 dlog_get_rtc_time_ms(void)
{
    // dlog会多次调用这个接口获取系统时间戳, 需要应用层维护一个全局时间戳, 要处理好网络时间和本地时间的同步
    // 返回的时间单位是毫秒, 24小时制
    //如当前时间为 20:13:30.100, 则返回 ((21 * 60 + 13) * 60) + 30) * 1000 + 100 = 76410100毫秒
    u32 time_ms = get_sys_time_ms();  // get_sys_time_ms函数需要自行实现, 此处仅示例
    return time_ms;
}
#endif

#endif
