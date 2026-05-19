#include "cpu/includes.h"
#include "asm/csfr.h"
#include "sdk_config.h"
#include "debug_record/debug_record.h"
#include "asm/sfc_norflash_api.h"
#include "boot.h"
#include "fs/resfile.h"
#include "fs/sdfile.h"
#include "init.h"
#if (defined(TCFG_CONFIG_DEBUG_RECORD_ENABLE) && (TCFG_CONFIG_DEBUG_RECORD_ENABLE == 1))

/* ---------------------------------------------------------------------------- */
/**
 * DEBUG_EXCEPTION_FILE_PATH:
 *    打开 flash 文件的路径, 可以根据 sdk 实际支持的文件类型进行路径的设置
 * DEBUG_EXCEPTION_FILE_OFFSET:
 *    如果复用已存在的区域,则需要把异常信息放到复用区域的末尾并且设置偏移
 */
/* ---------------------------------------------------------------------------- */
#define DEBUG_EXCEPTION_FILE_PATH 	  "flash/app/exif" // 保留区域不够, 将exif区域增加多4K, 复用exif区域的尾部4K来存储exception信息, 需要将CONFIG_EXIF_LEN增加4K去存储异常信息
#define DEBUG_EXCEPTION_FILE_OFFSET   (0x1000)         // 原本exif区域占用的空间, 需要减去该空间, 避免改写到exif原本的数据; 如果没有复用则填 0

/* #define DEBUG_EXCEPTION_FILE_PATH  		"flash/app/debug" */
/* #define DEBUG_EXCEPTION_FILE_OFFSET 	0 */

#define DEBUG_EXCEPTION_LOG_ENABLE
#ifdef DEBUG_EXCEPTION_LOG_ENABLE
#define log_info(format, ...)        printf(format, ## __VA_ARGS__)
#define log_error(format, ...)       printf(format, ## __VA_ARGS__)
#else
#define log_info(format, ...)
#define log_error(format, ...)
#endif

struct debug_flash_file_info {
    u32 file_addr;
    u32 file_len;
};

static struct debug_flash_file_info _flash_zone_info;

// 打开文件, 获取flash存储空间
static bool debug_exception_get_flash_zone_demo(struct debug_flash_file_info *flash_info)
{
    flash_info->file_addr = 0;
    flash_info->file_len = 0;

#if 1  // 可以根据sdk支持的文件类型使用不同的接口

    // 打开用于保存异常信息的文件
    RESFILE *fp = resfile_open(DEBUG_EXCEPTION_FILE_PATH);
    if (fp == NULL) {
        log_error("Open %s record file fail", DEBUG_EXCEPTION_FILE_PATH);
        return false;
    }

    struct resfile_attrs file_attr = {0};
    if (resfile_get_attrs(fp, &file_attr) != RESFILE_ERR_NONE) {
        return false;
    }
    flash_info->file_addr = file_attr.sclust + DEBUG_EXCEPTION_FILE_OFFSET;
    flash_info->file_len = file_attr.fsize - DEBUG_EXCEPTION_FILE_OFFSET;

    resfile_close(fp);

#else
    // 打开用于保存异常信息的文件
    FILE *fp = fopen(DEBUG_EXCEPTION_FILE_PATH, "wr");
    if (fp == NULL) {
        log_error("Open %s record file fail", DEBUG_EXCEPTION_FILE_PATH);
        goto __record_end;
    }
    struct vfs_attr file_attr = {0};
    fget_attrs(fp, &file_attr);
    flash_info->file_addr = file_attr.sclust;
    flash_info->file_len = file_attr.fsize;

    fclose(fp);
#endif

    log_info("open flash file addr 0x%x, len %d\n", flash_info->file_addr, flash_info->file_len);

    return true;
}

// 提供给debug_record模块内部使用的回调, 预留接口, 暂时未使用
static void debug_exception_get_flash_info_demo(u32 *file_addr, u32 *file_len)
{
    struct debug_flash_file_info flash_zone_info;

    debug_exception_get_flash_zone_demo(&flash_zone_info);

    *file_addr = flash_zone_info.file_addr;
    *file_len = flash_zone_info.file_len;
}

// 擦除保存在flash的异常信息
void debug_exception_flash_info_zone_erase(struct debug_flash_file_info flash_info)
{
    DEBUG_RECORD_ENABLE_CHECK_BEGIN();

    if ((flash_info.file_len == 0) || (flash_info.file_addr == 0)) {
        return;
    }

    u32 erase_addr = sdfile_cpu_addr2flash_addr(flash_info.file_addr);
    u32 erase_unit = 4096;
    u32 erase_size = 0;
    u32 total_size = flash_info.file_len;
    u32 erase_cmd = IOCTL_ERASE_SECTOR;
    if (get_boot_info()->vm.align == 1) {
        erase_unit = 256;
        erase_cmd = IOCTL_ERASE_PAGE;
    }
    do {
        int ret = norflash_ioctl(NULL, erase_cmd, erase_addr);
        printf("norflash_ioctl ret %d , 0x%x, %d, %d", ret, erase_addr, erase_size, total_size);
        erase_addr += erase_unit;
        erase_size += erase_unit;
    } while (erase_size < total_size);

    DEBUG_RECORD_ENABLE_CHECK_END();
}

// 将异常信息保存到flash中
static int debug_exception_info_save_2_flash_demo(struct debug_record_head *info, u16 info_len)
{
    struct debug_flash_file_info flash_zone_info;

    debug_exception_get_flash_zone_demo(&flash_zone_info);

    if ((flash_zone_info.file_len >= info_len) && (flash_zone_info.file_addr > 0)) {

        u32 *ptr = (u32 *)(flash_zone_info.file_addr);
        u8 dirty = 0;
        u32 write_addr = sdfile_cpu_addr2flash_addr(flash_zone_info.file_addr); //flash addr

        //检验flash区域是否干净:
        for (u32 i = 0; i < (flash_zone_info.file_len / 4); i++) {
            if (ptr[i] != 0xFFFFFFFF) {
                dirty = 1;
                break;
            }
        }
        if (dirty) {
            debug_exception_flash_info_zone_erase(flash_zone_info);
        }

        //写flash:
        norflash_write(NULL, (void *)info, info_len, write_addr);

    }

    return 0;
}

// 异常回调函数
static void debug_exception_record_handler_demo(void)
{
    DEBUG_RECORD_ENABLE_CHECK_BEGIN();

    u16 info_buf_len;
    // 获取异常信息
    void *buf = debug_record_2_ram();
    if (buf == NULL) {
        goto __exit_debug;
    }

    struct debug_record_head *info_buf = (struct debug_record_head *)buf;

    // 解析异常信息, 判断信息是否有效并且获取长度
    int ret = debug_record_parse(info_buf, 0, &info_buf_len); //只校验crc
    if (ret != DEBUG_RECORD_DATA_OK) {
        goto __exit_debug;
    }

    // 保存异常信息到flash
    debug_exception_info_save_2_flash_demo(info_buf, info_buf_len);

__exit_debug:

    memset(buf, 0, sizeof(struct debug_record_head));

    cpu_reset();

    DEBUG_RECORD_ENABLE_CHECK_END();
}

// 解析保存在flash中的异常信息
int debug_exception_record_parse_demo(void)
{
    DEBUG_RECORD_ENABLE_CHECK_BEGIN();

    u16 info_buf_len;
    int ret;

    // 打开保存在flash的异常数据
    debug_exception_get_flash_zone_demo(&_flash_zone_info);

    // 解析并打印保存在flash的异常数据
    ret = debug_record_parse((u8 *)(_flash_zone_info.file_addr), 1, &info_buf_len); //输出信息
    if (ret != DEBUG_RECORD_DATA_OK) {
        log_error("flash exception info parse err : ret = %d", ret);
        return ret;
    }

    // 擦除flash中的异常信息
    debug_exception_flash_info_zone_erase(_flash_zone_info);

    DEBUG_RECORD_ENABLE_CHECK_END();

    return DEBUG_RECORD_DATA_OK;
}

// 初始化, 注册回调
static int debug_exception_record_init_demo(void)
{
    DEBUG_RECORD_ENABLE_CHECK_BEGIN();

    debug_exception_record_handler_register(debug_exception_record_handler_demo);
    debug_exception_get_flash_zone_callback_register(debug_exception_get_flash_info_demo);

    DEBUG_RECORD_ENABLE_CHECK_END();

    return 0;
}

/* ---------------------------------------------------------------------------- */
/**
 * 该demo模块使用说明:
 *    1, 打开下面的接口可以使能自定义的异常信息处理流程, 反之则采样默认的异常信息处理流程
 *       early_initcall(debug_exception_record_init_demo);
 *    2, 打开下面的接口可以在开机时分析异常信息
 *       platform_initcall(debug_exception_record_parse_demo);
 */
/* ---------------------------------------------------------------------------- */

// debug_exception_record_init_demo 的初始化方式二选一:
//    1,可以采用 early_initcall 的方式初始化
//    2,可以手动在外部程序流程调用进行初始化, 注意需要在 do_platform_initcall 之前进行初始化
/* early_initcall(debug_exception_record_init_demo); */

// debug_exception_record_init_demo 的初始化方式二选一:
//    1,可以采用 platform_initcall 的方式初始化, 在上电时分析异常信息
//    2,可以手动在外部程序流程调用进行初始化
/* platform_initcall(debug_exception_record_parse_demo); */

#endif
