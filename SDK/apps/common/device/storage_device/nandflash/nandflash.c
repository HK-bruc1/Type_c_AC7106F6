#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".nandflash.data.bss")
#pragma data_seg(".nandflash.data")
#pragma const_seg(".nandflash.text.const")
#pragma code_seg(".nandflash.text")
#endif
#include "nandflash.h"
#include "app_config.h"
#include "clock.h"
#include "asm/wdt.h"
#include "flash_translation_layer/ftl_api.h"

#if TCFG_NANDFLASH_DEV_ENABLE

#undef LOG_TAG_CONST
#define LOG_TAG     "[nandFLASH]"
#define LOG_ERROR_ENABLE
//#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

#include "stddef.h"
#include "stdint.h"


struct nand_init_cmd_t {
    uint8_t addr;
    uint8_t data;
    uint8_t mask;
};
struct user_oob_info_t {
    uint8_t oob_offset;
    uint8_t oob_size;
};

struct spi_io_params_t {
    uint8_t port_cs_hd: 2;
    uint8_t port_clk_hd: 2;
    uint8_t port_do_hd: 2;
    uint8_t port_di_hd: 2;
    uint8_t port_d2_hd: 2;
    uint8_t port_d3_hd: 2;
};

struct nand_flash_params_t {

    uint32_t id;
    u32 plane_number;
    u32 block_number;
    u32 page_number;
    u32 page_size;

    struct spi_io_params_t spi_io_params;

    struct user_oob_info_t oob_info[2];

    uint8_t oob_size;

    uint8_t ecc_mask;
    uint16_t  ecc_status_0;
    uint16_t  ecc_status_1;

    uint8_t write_enable_position;  //写使能的位置，1表示在写入缓存区指令前进行写使能，0表示在写入缓存区指令后写使能
    uint8_t dummy_cycles;
    uint8_t init_cmd_size;
    struct nand_init_cmd_t init_cmd[4];
    uint8_t unlock_cmd_size;
    struct nand_init_cmd_t unlock_cmd[2];
};


#define NAND_GET_FEATURES         0x0F
#define NAND_SET_FEATURES         0x1F

//<features addr
#define NAND_FEATURES_PROTECT     0xA0
#define NAND_FEATURES             0xB0
#define NAND_GET_STATUS           0xC0

//< status reg
#define NAND_STATUS_OIP         BIT(0)
#define NAND_STATUS_WEL         BIT(1)
#define NAND_STATUS_E_FAIL      BIT(2)
#define NAND_STATUS_P_FAIL      BIT(3)
//< read
#define NAND_PAGE_READ_CACHE        0x13
#define NAND_READ_CACHE_x1          0x03
#define NAND_READ_CACHE_x2          0x3B
#define NAND_READ_CACHE_x2_IO       0xBB
#define NAND_READ_CACHE_x4          0x6B
#define NAND_READ_CACHE_x4_IO       0xEB

#define     NAND_FLASH_BLOCK_SIZE   (128*1024)

#define MAX_NANDFLASH_PART_NUM      4

static u8 g_nand_read_mode =  NAND_READ_CACHE_x1;
static const struct nand_flash_params_t *g_nand_flash_params;
static const struct spi_platform_data *g_spi_ctx;

static u8 spi_data_width = SPI_MODE_BIDIR_1BIT;

struct nandflash_partition {
    const char *name;
    u32 start_addr;
    u32 size;
    struct device device;
};

static struct nandflash_partition nor_part[MAX_NANDFLASH_PART_NUM];

struct nandflash_info {
    u32 flash_id;
    int spi_num;
    int spi_err;
    u8 spi_cs_io;
    u8 spi_r_width;
    u8 part_num;
    u8 open_cnt;
    struct nandflash_partition *const part_list;
    OS_MUTEX mutex;
    u32 max_end_addr;
};

static struct nandflash_info _nandflash = {
    .spi_num = (int) - 1,
    .part_list = nor_part,
};
static u8 spi_port[6];

u32 get_nandflash_capacity()
{
    return g_nand_flash_params->plane_number *  g_nand_flash_params->block_number * g_nand_flash_params->page_number * g_nand_flash_params->page_size;
}

static void nand_flash_dump_params()
{
    printf("g_nand_flash_params: %x\n", (u32)g_nand_flash_params);
    printf("Read Mode: %x\n", g_nand_read_mode);
    printf("ID: 0x%x\n", g_nand_flash_params->id);
    printf("Plane Number: %x\n", g_nand_flash_params->plane_number);
    printf("Block Number: %x\n", g_nand_flash_params->block_number);
    printf("Page Number: %x\n", g_nand_flash_params->page_number);
    printf("Page Size: %x\n", g_nand_flash_params->page_size);

    for (int i = 0; i < 2; ++i) {
        printf("OOB Info %d:\n", i);
        printf("  OOB Offset: %x\n", g_nand_flash_params->oob_info[i].oob_offset);
        printf("  OOB Size: %x\n", g_nand_flash_params->oob_info[i].oob_size);
    }

    printf("OOB Size: %x\n", g_nand_flash_params->oob_size);
    printf("ECC Mask: 0x%x\n", g_nand_flash_params->ecc_mask);
    printf("ECC Status 1: 0x%x\n", g_nand_flash_params->ecc_status_1);
    printf("ECC Status 0: 0x%x\n", g_nand_flash_params->ecc_status_0);
    printf("Write Enable Position: %x\n", g_nand_flash_params->write_enable_position);
    printf("Dummy Cycles: %x\n", g_nand_flash_params->dummy_cycles);
    printf("Init Command Size: %x\n", g_nand_flash_params->init_cmd_size);

    for (int i = 0; i < 4; ++i) {
        printf("Init Command %d:\n", i);
        printf("  Address: 0x%x\n", g_nand_flash_params->init_cmd[i].addr);
        printf("  Data: 0x%x\n", g_nand_flash_params->init_cmd[i].data);
        printf("  Mask: 0x%x\n", g_nand_flash_params->init_cmd[i].mask);
    }

    printf("Unlock Command Size: %x\n", g_nand_flash_params->unlock_cmd_size);

    for (int i = 0; i < 2; ++i) {
        printf("Unlock Command %d:\n", i);
        printf("  Address: 0x%x\n", g_nand_flash_params->unlock_cmd[i].addr);
        printf("  Data: 0x%x\n", g_nand_flash_params->unlock_cmd[i].data);
        printf("  Mask: 0x%x\n", g_nand_flash_params->unlock_cmd[i].mask);
    }

    {
        printf("0x%x=", g_nand_flash_params->id);
        printf("%d,", g_nand_flash_params->plane_number);
        printf("%d,", g_nand_flash_params->block_number);
        printf("%d,", g_nand_flash_params->page_number);
        printf("%d,HEX", g_nand_flash_params->page_size);
        const u8 *p = (const u8 *)&g_nand_flash_params->spi_io_params;
        uint32_t args_len = sizeof(*g_nand_flash_params) - offsetof(typeof(*g_nand_flash_params), spi_io_params);
        for (int i = 0; i < args_len; i++) {
            u8 dat = p[i];
            put_u4hex(dat >> 4);
            put_u4hex(dat);
        }
        putchar(';');
        putchar('\n');
    }
}


#define spi_cs_init() \
    do { \
        gpio_set_mode(IO_PORT_SPILT(_nandflash.spi_cs_io), PORT_OUTPUT_HIGH); \
    } while (0)

#define spi_cs_uninit() \
    do { \
        gpio_set_mode(IO_PORT_SPILT(_nandflash.spi_cs_io), PORT_HIGHZ); \
    } while (0)
#define spi_cs_h()                             gpio_write(_nandflash.spi_cs_io, 1)
#define spi_cs_l()                             gpio_write(_nandflash.spi_cs_io, 0)
#define spi_read_byte(data_width)              nand_spi_read_byte(data_width)
#define spi_write_byte(x,data_width)           nand_spi_write_byte(x,data_width)
#define spi_dma_read(x, y,data_width)          nand_spi_dma_read(x,y,data_width)
#define spi_dma_write(x, y,data_width)         nand_spi_dma_wirte(x,y,data_width)
#define spi_set_width(x)                       spi_set_bit_mode(_nandflash.spi_num, x)

void nand_spi_write_byte(u8 byte, u8 data_width)
{
    spi_set_width(data_width);
    spi_send_byte(_nandflash.spi_num, byte);
}

uint8_t nand_spi_read_byte(u8 data_width)
{
    spi_set_width(data_width);
    return spi_recv_byte(_nandflash.spi_num, &_nandflash.spi_err);

}

void nand_spi_dma_read(void *buf, u32 len, u8 data_width)
{
    spi_set_width(data_width);
    spi_dma_recv(_nandflash.spi_num, buf, len);

}

void nand_spi_dma_wirte(void *buf, u32 len, u8 data_width)
{
    spi_set_width(data_width);
    spi_dma_send(_nandflash.spi_num, buf, len);

}

static void nand_flash_set_spi_io_params(const struct spi_io_params_t *io)
{
    gpio_set_drive_strength(IO_PORT_SPILT(spi_port[0]), io->port_clk_hd);
    gpio_set_drive_strength(IO_PORT_SPILT(spi_port[1]), io->port_do_hd);
    gpio_set_drive_strength(IO_PORT_SPILT(spi_port[2]), io->port_di_hd);
    gpio_set_drive_strength(IO_PORT_SPILT(spi_port[3]), io->port_d2_hd);
    gpio_set_drive_strength(IO_PORT_SPILT(spi_port[4]), io->port_d3_hd);
    gpio_set_drive_strength(IO_PORT_SPILT(spi_port[5]), io->port_cs_hd);
}

static uint8_t nand_flash_get_features(uint32_t addr)
{
    uint8_t cmd[2] = {
        NAND_GET_FEATURES,
        addr,
    };
    spi_cs_l();
    spi_write_byte(cmd[0], SPI_MODE_BIDIR_1BIT);
    spi_write_byte(cmd[1], SPI_MODE_BIDIR_1BIT);
    uint8_t status = spi_read_byte(SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
    return status;
}

static uint8_t nand_flash_set_features(uint32_t addr, uint8_t status)
{
    uint8_t cmd[2] = {
        NAND_SET_FEATURES,
        addr,
    };
    spi_cs_l();
    spi_write_byte(cmd[0], SPI_MODE_BIDIR_1BIT);
    spi_write_byte(cmd[1], SPI_MODE_BIDIR_1BIT);
    spi_write_byte(status, SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
    return status;
}

static uint32_t nand_flash_updata_reg(uint32_t cmd_size, const struct nand_init_cmd_t *init_cmd)
{
    for (int i = 0; i < cmd_size; i++) {

        uint8_t sr_data =  nand_flash_get_features(init_cmd[i].addr);

        if ((sr_data & init_cmd[i].mask)  == init_cmd[i].data) {
            continue;
        }

        uint8_t data = (sr_data & (~init_cmd[i].mask)) | init_cmd[i].data;
        nand_flash_set_features(init_cmd[i].addr, data);
    }

    return 0;
}

static uint32_t nand_ecc_offset(u32 ecc_mask)
{
    for (int i = 0; i < 8; i++) {
        if (BIT(i) & ecc_mask) {
            return i;
        }
    }
    return 0;
}

static void nand_flash_soft_reset(void)
{
    spi_cs_l();
    spi_write_byte(0xff, SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
    mdelay(1);
}

static NAND_Result nand_flash_wait_idle(uint32_t timeout_ms)
{
    int timeout = timeout_ms * 500;
    uint8_t status = 0;

    while (timeout > 0) {
        timeout--;

        status = nand_flash_get_features(NAND_GET_STATUS);
        if (status & NAND_STATUS_OIP) {
            //udelay(100);
            continue;
        }

        if (g_nand_flash_params == NULL) {
            return NAND_SUCCESS;
        }

        uint8_t ecc_status = status & g_nand_flash_params->ecc_mask;
        uint32_t ecc_offset = nand_ecc_offset(g_nand_flash_params->ecc_mask);
        ecc_status >>=  ecc_offset;
        uint8_t ecc = ((BIT(ecc_status) & g_nand_flash_params->ecc_status_1) ? 2 : 0) | \
                      ((BIT(ecc_status) & g_nand_flash_params->ecc_status_0) ? 1 : 0);

        if (status) {
            log_debug("C0_status:%x ecc_status: %x -> ecc: %x ecc_offset:%d ecc_mask:%x %x %x",
                      status, ecc_status, ecc, ecc_offset, g_nand_flash_params->ecc_mask,
                      g_nand_flash_params->ecc_status_1, g_nand_flash_params->ecc_status_0);
        }

        if (status & NAND_STATUS_E_FAIL) {
            log_error("erase fail");
            nand_flash_soft_reset();
            return NAND_ERROR_E_FAIL;
        }

        if (status & NAND_STATUS_P_FAIL) {
            log_error("program fail");
            nand_flash_soft_reset();
            return NAND_ERROR_P_FAIL;
        }

        switch (ecc) {
        case 0:
            return NAND_SUCCESS;

        case 1: // 少量bit ecc错误，不处理
            log_error("ecc corrected");
            return  NAND_SUCCESS;

        case 2: // 多bit ecc错误
            log_error("too many ecc errors");
            return NAND_ECC_CORRECTED;

        case 3: //ecc 无法纠正
            log_error("ecc error");
            return NAND_ERROR_ECC;
            /* return NAND_ECC_CORRECTED; */
        }

    }

    log_error("timeout C0_status %x", status);
    return NAND_ERROR_TIME_OUT;
}


static uint32_t nand_flash_read_id(void)
{
    spi_cs_l();
    spi_write_byte(0x9f, SPI_MODE_BIDIR_1BIT);
    spi_write_byte(0x00, SPI_MODE_BIDIR_1BIT);
    uint32_t id = spi_read_byte(SPI_MODE_BIDIR_1BIT);
    id <<= 8;
    id |= spi_read_byte(SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
    return id;
}

static NAND_Result nand_flash_page2cache(uint32_t address)
{
    uint8_t cmd[4];

    cmd[0] = NAND_PAGE_READ_CACHE;
    cmd[1] = address >> 16;
    cmd[2] = address >> 8;
    cmd[3] = address;

    spi_cs_l();
    spi_dma_write(cmd, 4, SPI_MODE_BIDIR_1BIT);
    spi_cs_h();

    return nand_flash_wait_idle(5); //10ms
}

static void nand_read_cache(uint32_t rcmd, uint32_t column_address, uint8_t *data, uint16_t data_size, uint32_t data_width)
{
    uint8_t cmd[4];
    cmd[0] = rcmd;
    cmd[1] = column_address >> 8;
    cmd[2] = column_address;
    cmd[3] = 0xff; //dummy

    u8 spi_r_width = 1;
    switch (data_width) {
    case 1:
        spi_r_width = SPI_MODE_BIDIR_1BIT;
        break;
    case 2:
        spi_r_width = SPI_MODE_UNIDIR_2BIT;
        break;
    case 4:
        spi_r_width = SPI_MODE_UNIDIR_4BIT;
        break;
    }

    spi_cs_l();
#if __USE_NAND_SFC_DMA
    if (((column_address & 0xfff) % 4) || (data_size % 4) || ((data_width != 4))) {
        spi_dma_write(cmd, 4, SPI_MODE_BIDIR_1BIT);
        spi_dma_read(data, data_size, spi_r_width);
    } else {
        spi_dma_write(cmd, 3, SPI_MODE_BIDIR_1BIT); //dummy 由sfc控制器发送
        nand_sfc_dma_read(data, data_size, spi_width);
    }
#else
    spi_dma_write(cmd, 4, SPI_MODE_BIDIR_1BIT);

    spi_dma_read(data, data_size, spi_r_width);
#endif
    spi_cs_h();
}

NAND_Result nand_flash_page_read(uint32_t block, uint32_t page, uint32_t column_address, uint8_t *data, uint32_t data_size)
{
    if (block > g_nand_flash_params->block_number) {
        log_error("%s() inv block addr %x", __func__, block);
        return NAND_ERROR_ADDR;
    }
    uint32_t row_address = (block << 6) | page;

    if (g_nand_flash_params->plane_number == 2) {
        if (block % 2) {
            column_address |= BIT(12);
        }
    }

    log_debug("%s [%xH] RA(%x) blk: %x page: %x CA(%x) data_size:%x", __func__, g_nand_read_mode, row_address, block, page, column_address, data_size);

    NAND_Result ret = nand_flash_page2cache(row_address);

    /* if (ret >= NAND_ERROR_ECC) { */
    /* strcpy((char *)data, "ecc_error"); */
    /* memset(data + 0x10, ret, 10); */
    /* memcpy(data + 0x20, g_nand_flash_params, sizeof(*g_nand_flash_params)); */
    /* return ret; */
    /* } */

    uint32_t data_width = 1;

    switch (g_nand_read_mode) {
    case NAND_READ_CACHE_x1:
        break;

    case NAND_READ_CACHE_x2:
        data_width = 2;
        break;

    case NAND_READ_CACHE_x4:
        data_width = 4;
        break;
    }

    nand_read_cache(g_nand_read_mode, column_address, data, data_size, data_width);

    return ret;
}

static NAND_Result nand_flash_read(uint32_t addr, uint8_t *data, uint32_t data_size)
{
    NAND_Result ret = NAND_SUCCESS;
    while (data_size) {
        uint32_t block_addr = addr / NAND_FLASH_BLOCK_SIZE;
        uint32_t page_addr = (addr % NAND_FLASH_BLOCK_SIZE) / g_nand_flash_params->page_size;
        uint32_t offset = (addr % NAND_FLASH_BLOCK_SIZE) % g_nand_flash_params->page_size;
        uint32_t cnt = g_nand_flash_params->page_size - offset;
        cnt = MIN(cnt, data_size);

        ret = nand_flash_page_read(block_addr, page_addr, offset, data, cnt);

        if (ret >= NAND_ERROR_ECC) {
            return ret;
        }

        addr += cnt;
        data += cnt;
        data_size -= cnt;
    }

    return ret;
}

static void nand_flash_write_enable()
{
    spi_cs_l();
    spi_write_byte(0x06, SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
}

NAND_Result nand_flash_erase(uint32_t addr)
{
    uint8_t cmd[4];

    uint32_t block_addr = addr / NAND_FLASH_BLOCK_SIZE;

    if (addr % NAND_FLASH_BLOCK_SIZE) {
        log_error("%s() inv erase addr %x", __func__, addr);
        return NAND_ERROR_ADDR;
    }

    log_debug("%s() %x %x ", __func__, addr, block_addr);

    if (block_addr > g_nand_flash_params->block_number) {
        log_error("%s() inv erase addr %x", __func__, addr);
        return NAND_ERROR_ADDR;
    }

    nand_flash_write_enable();
    uint32_t row_address = (block_addr << 6) | 0;
    cmd[0] = 0xD8;
    cmd[1] = row_address >> 16;
    cmd[2] = row_address >> 8;
    cmd[3] = row_address;

    spi_cs_l();
    spi_dma_write(cmd, 4, SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
    NAND_Result ret = nand_flash_wait_idle(10);
    return ret;
}

static void nand_flash_erase_all()
{
    for (int i = 0; i < g_nand_flash_params->block_number; i++) {
        nand_flash_erase(i * NAND_FLASH_BLOCK_SIZE);
    }
}

static void nand_flash_program_load(uint32_t column_address, uint8_t *data, uint32_t data_size, uint32_t data_width)
{
    uint8_t cmd[3];
    cmd[0] = data_width == 4 ? 0x32 : 0x02;
    cmd[1] = column_address >> 8;
    cmd[2] = column_address;

    spi_cs_l();
    spi_dma_write(cmd, 3, SPI_MODE_BIDIR_1BIT);
    spi_dma_write(data, data_size, data_width == 4 ? SPI_MODE_UNIDIR_4BIT : SPI_MODE_BIDIR_1BIT);
    spi_cs_h();
}

NAND_Result nand_flash_page_write(uint32_t block, uint32_t page, uint32_t column_address, uint8_t *data, uint32_t data_size)
{
    if (block > g_nand_flash_params->block_number) {
        log_error("%s() inv block addr %x", __func__, block);
        return NAND_ERROR_ADDR;
    }

    if (g_nand_flash_params->write_enable_position) {
        nand_flash_write_enable();
    }

    uint32_t data_width = 1;

    switch (g_nand_read_mode) {
    case NAND_READ_CACHE_x1:
        break;

    case NAND_READ_CACHE_x2:
        data_width = 1;
        break;

    case NAND_READ_CACHE_x4:
        data_width = 4;
        break;
    }

    if (g_nand_flash_params->plane_number == 2) {
        if (block % 2) {
            column_address |= BIT(12);
        }
    }

    nand_flash_program_load(column_address, data, data_size, data_width);

    if (g_nand_flash_params->write_enable_position == 0) {
        nand_flash_write_enable();
    }

    uint8_t cmd[4];
    uint32_t row_address = (block << 6) | page;
    cmd[0] = 0x10;
    cmd[1] = row_address >> 16;
    cmd[2] = row_address >> 8;
    cmd[3] = row_address >> 0;

    log_debug("%s [%xH] RA(%x) blk: %x page: %x CA(%x) data_size:%x", __func__, data_width == 4 ? 0x32 : 0x02, row_address, block, page, column_address, data_size);

    spi_cs_l();

    spi_dma_write(cmd, 4, SPI_MODE_BIDIR_1BIT);

    spi_cs_h();

    NAND_Result ret = nand_flash_wait_idle(10);
    return ret;
}

static NAND_Result nand_flash_write(uint32_t addr, uint8_t *data, uint32_t data_size)
{
    NAND_Result ret = NAND_SUCCESS;
    while (data_size) {
        uint32_t block_addr = addr / NAND_FLASH_BLOCK_SIZE;
        uint32_t page_addr = (addr % NAND_FLASH_BLOCK_SIZE) / g_nand_flash_params->page_size;
        uint32_t offset = addr % g_nand_flash_params->page_size;
        uint32_t cnt = g_nand_flash_params->page_size - offset;
        cnt = MIN(cnt, data_size);

        ret = nand_flash_page_write(block_addr, page_addr, offset, data, cnt);
        /* u32 row_address = (block_addr << 6)|page_addr;                              */
        /* ret = nand_flash_page2cache(row_address);                                   */
        /* if(ret == NAND_ERROR_ECC){                                                  */
        /*     while(1);                                                               */
        /* }                                                                           */

        if (ret >= NAND_ERROR_ECC) {
            return ret;
        }

        data += cnt;
        data_size -= cnt;
        addr += cnt;
    }

    return ret;
}

static u8 test_buf[0x800];
void nand_flash_test()
{
    nand_flash_read(0, test_buf, 0x800);
    log_info("befor write");
    printf_buf(test_buf, 32);
    for (int i = 0; i < sizeof(test_buf); i++) {
        test_buf[i] = i;
    }

    nand_flash_write(0, test_buf, 0x800);

    nand_flash_read(128 * 1024, test_buf, 0x800);
    printf_buf(test_buf, 32);

    log_info("after write");
    nand_flash_read(0, test_buf, 0x800);
    printf_buf(test_buf, 32);

    /* nand_flash_erase_all(); */
}

static void nand_flash_unlock(u32 is_write_protect)
{
    if (is_write_protect) {
        nand_flash_updata_reg(g_nand_flash_params->init_cmd_size, g_nand_flash_params->init_cmd);
    } else {
        nand_flash_updata_reg(g_nand_flash_params->unlock_cmd_size, g_nand_flash_params->unlock_cmd);
    }
}

static struct nandflash_partition *nandflash_find_part(const char *name)
{
    struct nandflash_partition *part = NULL;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if (!strcmp(part->name, name)) {
            return part;
        }
    }
    return NULL;
}

static struct nandflash_partition *nandflash_new_part(const char *name, u32 addr, u32 size)
{
    struct nandflash_partition *part;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            break;
        }
    }
    if (part->name != NULL) {
        log_error("create nandflash part fail\n");
        return NULL;
    }
    memset(part, 0, sizeof(*part));
    part->name = name;
    part->start_addr = addr;
    part->size = size;
    if (part->start_addr + part->size > _nandflash.max_end_addr) {
        _nandflash.max_end_addr = part->start_addr + part->size;
    }
    _nandflash.part_num++;
    return part;
}

static void nandflash_delete_part(const char *name)
{
    struct nandflash_partition *part;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if (!strcmp(part->name, name)) {
            part->name = NULL;
            _nandflash.part_num--;
        }
    }
}

static int nandflash_verify_part(struct nandflash_partition *p)
{
    struct nandflash_partition *part = NULL;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if ((p->start_addr >= part->start_addr) && (p->start_addr < part->start_addr + part->size)) {
            if (strcmp(p->name, part->name) != 0) {
                return -1;
            }
        }
    }
    return 0;
}



int nand_flash_init(void *arg);
int _nandflash_init(const char *name, struct nandflash_dev_platform_data *pdata)
{
    log_info("nandflash_init ! %x %x", pdata->spi_cs_port, pdata->spi_read_width);
    if (_nandflash.spi_num == (int) - 1) {
        _nandflash.spi_num = pdata->spi_hw_num;
        _nandflash.spi_cs_io = pdata->spi_cs_port;
        _nandflash.spi_r_width = pdata->spi_read_width;
        _nandflash.flash_id = 0;
        os_mutex_create(&_nandflash.mutex);
        _nandflash.max_end_addr = 0;
        _nandflash.part_num = 0;
        spi_data_width = pdata->spi_pdata->mode;

        memcpy(spi_port, pdata->spi_pdata->port, 6);
        spi_port[5] = pdata->spi_cs_port;

        gpio_set_drive_strength(IO_PORT_SPILT(pdata->spi_pdata->port[0]), PORT_DRIVE_STRENGT_64p0mA);
    }
    ASSERT(_nandflash.spi_num == pdata->spi_hw_num);
    ASSERT(_nandflash.spi_cs_io == pdata->spi_cs_port);
    ASSERT(_nandflash.spi_r_width == pdata->spi_read_width);
    struct nandflash_partition *part;
    part = nandflash_find_part(name);
    if (!part) {
        part = nandflash_new_part(name, pdata->start_addr, pdata->size);
        ASSERT(part, "not enough nandflash partition memory in array\n");
        ASSERT(nandflash_verify_part(part) == 0, "nandflash partition %s overlaps\n", name);
        log_info("nandflash new partition %s\n", part->name);
    } else {
        ASSERT(0, "nandflash partition name already exists\n");
    }
    nand_flash_init(NULL);
    return 0;
}

static void clock_critical_enter()
{

}
static void clock_critical_exit()
{
    if (!(_nandflash.flash_id == 0 || _nandflash.flash_id == 0xffff)) {
        spi_set_baud(_nandflash.spi_num, spi_get_baud(_nandflash.spi_num));
    }
}
CLOCK_CRITICAL_HANDLE_REG(spi_nandflash, clock_critical_enter, clock_critical_exit);



#include "nandflash_params.c"
int nand_flash_init(void *arg)
{
    int reg = 0;
    os_mutex_pend(&_nandflash.mutex, 0);
    log_info("nandflash open\n");
    if (!_nandflash.open_cnt) {
        spi_cs_init();
        spi_open(_nandflash.spi_num, get_hw_spi_config(_nandflash.spi_num));
        spi_set_width(_nandflash.spi_r_width);//readX2X4

        if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
            g_nand_read_mode = NAND_READ_CACHE_x4;
        } else if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_2BIT) {
            g_nand_read_mode = NAND_READ_CACHE_x2;
        } else {
            g_nand_read_mode = NAND_READ_CACHE_x1;
        }

        const struct spi_io_params_t spi_io_params = {
            .port_cs_hd = 3,
            .port_clk_hd = 3,
            .port_do_hd = 3,
            .port_di_hd = 3,
            .port_d2_hd = 3,
            .port_d3_hd = 3,
        };

        nand_flash_set_spi_io_params(&spi_io_params);

        gpio_set_mode(IO_PORT_SPILT(spi_port[3]), PORT_INPUT_PULLUP_10K);
        gpio_set_mode(IO_PORT_SPILT(spi_port[4]), PORT_INPUT_PULLUP_10K);

        nand_flash_soft_reset();

        mdelay(5);

        nand_flash_wait_idle(150);

        uint32_t id = nand_flash_read_id();

        _nandflash.flash_id = id;

        for (int i = 0; i < ARRAY_SIZE(def_params); i++) {
            log_info("params[%d].ID:%x", i, def_params[i].id);
            if (def_params[i].id == id) {
                g_nand_flash_params = &def_params[i];
                break;
            }
        }

        log_info("ID:%x", id);
        if (id == 0 || id == 0xffff) {
            log_error("INVAL ID %x", id);
        }

        nand_flash_set_spi_io_params(&g_nand_flash_params->spi_io_params);

        if (g_nand_flash_params == NULL) {
            log_error("nand flash init failed can't find id %x \n", id);
            nand_flash_dump_params();
        } else {
            nand_flash_updata_reg(g_nand_flash_params->init_cmd_size, g_nand_flash_params->init_cmd);
            nand_flash_set_spi_io_params(&g_nand_flash_params->spi_io_params);
            nand_flash_unlock(0);
            /* nand_flash_erase_all(); */
        }

        if ((_nandflash.flash_id == 0) || (_nandflash.flash_id == 0xffff)) {
            log_error("read nandflash id error !\n");
            reg = -ENODEV;
            goto __exit;
        }
        log_info("nandflash open success !\n");
    }
    if (_nandflash.flash_id == 0 || _nandflash.flash_id == 0xffff)  {
        log_error("re-open nandflash id error !\n");
        reg = -EFAULT;
        goto __exit;
    }
    _nandflash.open_cnt++;
    /* nand_flash_erase_all();// */
__exit:
    os_mutex_post(&_nandflash.mutex);
    return reg;
}

int _nandflash_close(void)
{
    os_mutex_pend(&_nandflash.mutex, 0);
    log_info("nandflash close\n");
    if (_nandflash.open_cnt) {
        _nandflash.open_cnt--;
    }
    if (!_nandflash.open_cnt) {
        spi_deinit(_nandflash.spi_num);
        spi_cs_uninit();

        log_info("nandflash close done\n");
    }
    os_mutex_post(&_nandflash.mutex);
    return 0;
}


int _nandflash_ioctl(u32 cmd, u32 arg, u32 unit, void *_part)
{
    int reg = 0;
    struct nandflash_partition *part = _part;
    os_mutex_pend(&_nandflash.mutex, 0);
    switch (cmd) {
    case IOCTL_GET_STATUS:
        *(u32 *)arg = 1;
        /* *(u32 *)arg = nand_get_features(GD_GET_STATUS); */
        break;
    case IOCTL_GET_ID:
        *((u32 *)arg) = _nandflash.flash_id;
        break;
    case IOCTL_GET_BLOCK_SIZE:
        *(u32 *)arg = 512;//usb fat
        /* *(u32 *)arg = NAND_FLASH_BLOCK_SIZE; */
        break;
    case IOCTL_ERASE_BLOCK:
        if ((arg  + part->start_addr) % NAND_FLASH_BLOCK_SIZE == 0) {
            reg = nand_flash_erase(arg  + part->start_addr);
        }
        break;
    case IOCTL_GET_CAPACITY:
        *(u32 *)arg = get_nandflash_capacity();
        break;
    case IOCTL_FLUSH:
        break;
    case IOCTL_CMD_RESUME:
        break;
    case IOCTL_CMD_SUSPEND:
        break;
    case IOCTL_CHECK_WRITE_PROTECT:
        *(u32 *)arg = 0;
        break;
    default:
        reg = -EINVAL;
        break;
    }
    os_mutex_post(&_nandflash.mutex);
    return reg;
}


/*************************************************************************************
 *                                  挂钩 device_api
 ************************************************************************************/

static int nandflash_dev_init(const struct dev_node *node, void *arg)
{
    struct nandflash_dev_platform_data *pdata = arg;
    return _nandflash_init(node->name, pdata);
}

static int nandflash_dev_open(const char *name, struct device **device, void *arg)
{
    struct nandflash_partition *part;
    part = nandflash_find_part(name);
    if (!part) {
        log_error("no nandflash partition is found\n");
        return -ENODEV;
    }
    *device = &part->device;
    (*device)->private_data = part;
    if (atomic_read(&part->device.ref)) {
        return 0;
    }
    return nand_flash_init(arg);
}
static int nandflash_dev_close(struct device *device)
{
    return _nandflash_close();
}

//return: 0:err, =len:ok, <0:block err
static int nandflash_dev_read(struct device *device, void *buf, u32 len, u32 offset)
{
    int reg;
    offset = offset * 512;
    len = len * 512;
    /* r_printf("flash read sector = %d, num = %d\n", offset, len); */
    struct nandflash_partition *part;
    part = (struct nandflash_partition *)device->private_data;
    if (!part) {
        log_error("nandflash partition invalid\n");
        return -EFAULT;
    }
    offset += part->start_addr;
    reg = nand_flash_read(offset, buf, len);
    if (reg) {
        r_printf(">>>[r error]:\n");
        len = 0;
        if (reg < 0) {
            return reg;
        }
    }
    len = len / 512;
    return len;
}
//写前需确保该block(128k)已擦除
static int nandflash_dev_write(struct device *device, void *buf, u32 len, u32 offset)
{
    /* r_printf("flash write sector = %d, num = %d\n", offset, len); */
    int reg = 0;
    offset = offset * 512;
    len = len * 512;
    struct nandflash_partition *part = device->private_data;
    if (!part) {
        log_error("nandflash partition invalid\n");
        return -EFAULT;
    }
    offset += part->start_addr;
    reg = nand_flash_write(offset, buf, len);
    if (reg) {
        r_printf(">>>[w error]:\n");
        len = 0;
    }
    len = len / 512;
    return len;
}
static bool nandflash_dev_online(const struct dev_node *node)
{
    return 1;
}
static int nandflash_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    struct nandflash_partition *part = device->private_data;
    if (!part) {
        log_error("nandflash partition invalid\n");
        return -EFAULT;
    }
    return _nandflash_ioctl(cmd, arg, 512,  part);
}
const struct device_operations nandflash_dev_ops = {
    .init   = nandflash_dev_init,
    .online = nandflash_dev_online,
    .open   = nandflash_dev_open,
    .read   = nandflash_dev_read,
    .write  = nandflash_dev_write,//写前需确保该block(128k)已擦除
    .ioctl  = nandflash_dev_ioctl,
    .close  = nandflash_dev_close,
};

#include "flash_translation_layer/ftl_api.h"
void ftl_get_nand_info(struct ftl_nand_flash *ftl)
{
    ftl->block_begin = 0;
    ftl->logic_block_num = g_nand_flash_params->block_number * 98 / 100; //98%
    ftl->block_end = g_nand_flash_params->block_number;
    ftl->page_num = g_nand_flash_params->page_number;
    ftl->page_size = g_nand_flash_params->page_size;
    ftl->oob_size = g_nand_flash_params->oob_size;
    ftl->oob_user_size[0] = g_nand_flash_params->oob_info[0].oob_size;
    ftl->oob_user_size[1] = g_nand_flash_params->oob_info[1].oob_size;
    ftl->oob_user_offset[0] = g_nand_flash_params->oob_info[0].oob_offset;
    ftl->oob_user_offset[1] = g_nand_flash_params->oob_info[1].oob_offset;
    ftl->max_erase_cnt = 100000;
}
#endif


