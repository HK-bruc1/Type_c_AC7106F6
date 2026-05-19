#ifndef _DLOG_H_
#define _DLOG_H_

#include "typedef.h"

#define DLOG_STR_TAB_STRUCT_SIZE             (16)  // 决定了 dlog_str_tab_s 结构体的大小, 如果有修改结构体大小必需修改该宏和sdk_ld.c中的STR_TAB_SIZE值

struct dlog_str_tab_s {
    u32 arg_num : 8;
    u32 dlog_level : 8;
    u32 res0 : 16;
    u32 dlog_str_addr;
    u32 arg_type_bit_map;
    u32 res1;
} __attribute__((packed));

struct dlog_output_op_s {
    int (*dlog_output_init)(void);
    int (*dlog_output_get_zone)(u32 *addr, u32 *len);
    int (*dlog_output_zone_erase)(u16 erase_sector, u16 sector_num);
    int (*dlog_output_write)(void *buf, u16 len, u32 offset);
    int (*dlog_output_read)(void *buf, u16 len, u32 offset);
    int (*dlog_output_direct)(void *buf, u16 len);
};

#define REGISTER_DLOG_OPS(cfg, pri) \
	const struct dlog_output_op_s cfg SEC_USED(.dlog.pri.ops)

struct dlog_output_channel_s {
    int (*send)(const void *buf, u16 len);
    int (*is_busy)();
};

#define REGISTER_DLOG_OUT_CHANNLE(channel) \
	const struct dlog_output_channel_s dlog_out_ch_##channel SEC_USED(.dlog_out_channel)

#define LOG_BASE_UNIT_SIZE                (4 * 1024)  // 固定为 4K
#define LOG_MAX_ARG_NUM                   (14)        // 不可修改

enum DLOG_OUTPUT_TYPE {
    DLOG_OUTPUT_2_NONE  = 0,
    DLOG_OUTPUT_2_FLASH = (1 << 0),
    DLOG_OUTPUT_2_UART  = (1 << 1),
};

enum DLOG_LOG_TYPE {
    DLOG_LOG_2_NONE     = 0,
    DLOG_LOG_2_PRINT    = (1 << 0),
    DLOG_LOG_2_PUTCHAR  = (1 << 1),
    DLOG_LOG_2_PUT_BUF  = (1 << 2),
};

/*----------------------------------------------------------------------------*/
/**@brief dlog功能初始化
  @param  void
  @return 成功返回 0, 失败返回 < 0
  @note   dlog_init后dlog处于disable状态,需要通过dlog_enable(1)去使能dlog
 */
/*----------------------------------------------------------------------------*/
int dlog_init(void);

/*----------------------------------------------------------------------------*/
/**@brief dlog功能的关闭(与dlog_init对应)
  @param  void
  @return void
  @note   禁止在关中断/中断下调用该函数, 否则可能丢失ram中数据
 */
/*----------------------------------------------------------------------------*/
void dlog_close(void);

/*----------------------------------------------------------------------------*/
/**@brief dlog功能的临时使能/失能
  @param  en
            0 : 禁止打印数据输出(禁止输出到缓冲区)
            1 : 使能打印数据输出(使能输出到缓冲区)
  @return void
  @note   只是临时开关打印数据输出, 不刷新缓冲区数据到flash,(可自行调用flush接口)
 */
/*----------------------------------------------------------------------------*/
void dlog_enable(u8 en);

/*----------------------------------------------------------------------------*/
/**@brief 更新 ram 的数据到 flash 中
  @param  timeout 刷新等待时间
              0 : 不等待, 只发送消息给任务刷新dlog, 不管是否刷新成功, 都马上退出
              -1: 一直等待, 直到刷新成功才退出该函数
              xx: 刷新成功则马上退出, 否则等待 xx * 10ms的超时时间, 直到刷新成功
  @return 成功返回 0, 失败返回非 0
  @note   不可在中断/关中断传入非 0 参数值
 */
/*----------------------------------------------------------------------------*/
int dlog_flush2flash(u32 timeout);

/*----------------------------------------------------------------------------*/
/**@brief 读取flash中的dlog信息
  @param  buf    : 返回读取的数据
  @param  len    : 需要读取的数据长度
  @param  offset : 读取数据的偏移(偏移是相对于flash区域起始地址)
  @return 返回读取到的数据长度,0表示已读取完数据
  @note
 */
/*----------------------------------------------------------------------------*/
u16 dlog_read_from_flash(u8 *buf, u16 len, u32 offset);

/*----------------------------------------------------------------------------*/
/**@brief 读取flash中的dlog和异常数据
  @param  buf    : 返回读取的数据
  @param  len    : 需要读取的数据长度
  @param  offset : 读取数据的偏移(偏移是相对于flash区域起始地址)
  @return 返回读取到的数据长度,0表示已读取完数据
  @note
 */
/*----------------------------------------------------------------------------*/
u16 dlog_read_log_data(u8 *buf, u16 len, u32 offset);

/*----------------------------------------------------------------------------*/
/**@brief 设置dlog等级
  @param  level  : 等级, 如下宏定义所示
                        __LOG_VERB      0
                        __LOG_DEBUG     1
                        __LOG_INFO      2
                        __LOG_WARN      3
                        __LOG_ERROR     4
                        __LOG_CHAR      5
  @return void
  @note
 */
/*----------------------------------------------------------------------------*/
void dlog_level_set(int level);

/*----------------------------------------------------------------------------*/
/**@brief 获取dlog等级
  @param  void
  @return level  : 等级, 如下宏定义所示
                        __LOG_VERB      0
                        __LOG_DEBUG     1
                        __LOG_INFO      2
                        __LOG_WARN      3
                        __LOG_ERROR     4
                        __LOG_CHAR      5
  @note
 */
/*----------------------------------------------------------------------------*/
int dlog_level_get(void);

/*----------------------------------------------------------------------------*/
/**@brief 设置dlog的输出类型
  @param  type     :
                   DLOG_OUTPUT_2_NONE: 既不输出到串口也不输出到flash,仅输出到缓存
                   DLOG_OUTPUT_2_FLASH: 仅输出到flash
                   DLOG_OUTPUT_2_UART: 仅输出到串口
  @return : 如果返回值不为0, 则表示缓存数据没有刷新到flash(非丢数据)
  @note 需要同时输出到串口和flash可用位或(DLOG_OUTPUT_2_UART | DLOG_OUTPUT_2_FLASH)
 */
/*----------------------------------------------------------------------------*/
int dlog_output_type_set(enum DLOG_OUTPUT_TYPE type);

/*----------------------------------------------------------------------------*/
/**@brief 获取dlog的输出类型
  @param  void
  @return : enum DLOG_OUTPUT_TYPE
  @note
 */
/*----------------------------------------------------------------------------*/
enum DLOG_OUTPUT_TYPE dlog_output_type_get(void);

/*----------------------------------------------------------------------------*/
/**@brief 输出dlog的同步信息
  @param  void
  @return : 大于等于0表示执行成功
  @note 由于时间戳和序号采用增量形式,所以通过该接口可以输出一个绝对值进行同步
 */
/*----------------------------------------------------------------------------*/
int dlog_info_sync(void);

/*----------------------------------------------------------------------------*/
/**@brief 清零dlog的丢失log数量
  @param  void
  @return void
  @note
 */
/*----------------------------------------------------------------------------*/
void dlog_leaks_clear(void);

/*----------------------------------------------------------------------------*/
/**@brief 获取dlog的hash
  @param  void
  @return : hash 值
  @note 可以通过程序的hash值来匹配dlog.bin
 */
/*----------------------------------------------------------------------------*/
u16 dlog_get_head_hash(void);

/*----------------------------------------------------------------------------*/
/**@brief dlog用于获取rtc时间的年月日,需要外部重写实现,若不支持rtc时间可忽略
  @param  time : 用于保存返回的年月日
  @return : 大于等于0表示成功获取
  @note time 为 struct sys_time 结构体指针
 */
/*----------------------------------------------------------------------------*/
__attribute__((weak)) int dlog_get_rtc_time(void *time);

/*----------------------------------------------------------------------------*/
/**@brief dlog用于获取rtc时间的时分秒毫秒,需要外部重写实现,若不支持rtc时间可忽略
  @param  void
  @return : 返回时分秒毫秒组成的一个整数值,24小时制
  @note  如当前时间为 21:13:30.100, 则返回 ((21 * 60 + 13) * 60) + 30) * 1000 + 100 = 76410100毫秒
 */
/*----------------------------------------------------------------------------*/
__attribute__((weak)) u32 dlog_get_rtc_time_ms(void);

/*----------------------------------------------------------------------------*/
/**@brief 获取dlog是否有使能存储log到flash功能
  @param  void
  @return : 关闭dlog存储到flash或者无flash时均返回0, 其它返回1
  @note
 */
/*----------------------------------------------------------------------------*/
u8 dlog_flash_exsit_get(void);


void dlog_uart_auto_enable_init(void);

int dlog_uart_output_set(enum DLOG_OUTPUT_TYPE type);


int dlog_log_type_set(enum DLOG_LOG_TYPE type);

enum DLOG_LOG_TYPE dlog_log_type_get(void);

// 仅声明, 非外部调用接口
int dlog_print(const struct dlog_str_tab_s *str_tab, u32 arg_bit_map_and_num, const char *format, ...);
int dlog_putchar(char c);
int dlog_put_buf(const u8 *buf, int len);

extern const int config_dlog_enable;
extern const int config_ulog_enable;
extern const int dlog_seg_begin;

// 超过 20 个参数就会计算出错
#define VA_ARGS_NUM_PRIV(P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, P18 ,P19, P20, Pn, ...) Pn
#define VA_ARGS_NUM(...)   VA_ARGS_NUM_PRIV(-1, ##__VA_ARGS__, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define VA_ARGS_NUM_CHECK(num)  _Static_assert((num) <= LOG_MAX_ARG_NUM, "err: printf arg num too much\n")

#define DLOG_STR_TAB_SIZE_CHECK()  _Static_assert(sizeof(struct dlog_str_tab_s) <= DLOG_STR_TAB_STRUCT_SIZE, \
        "err: struct dlog_str_tab_s size must less than DLOG_STR_TAB_STRUCT_SIZE\n")

#define ARG_TYPE_4BYTE    char:0, unsigned char:0, signed char:0, short:0, unsigned short:0, int: 0, unsigned int: 0, unsigned long int: 0, long int: 0, \
                          void *: 0, short *:0, unsigned short *:0, int *: 0, unsigned int *: 0, unsigned long int *: 0, long int *: 0
#define ARG_TYPE_8BYTE    float: 1, double: 1, long long int: 1, unsigned long long int: 1
#define ARG_TYPE_STRING   char *: 2, unsigned char *: 2, const char *: 2, const unsigned char *: 2, char * const: 2, unsigned char * const: 2
// #define ARG_TYPE_STRING   const char *: 2, const unsigned char *: 2, char * const: 2, unsigned char * const: 2
#define ARG_TYPE_DEFAULT  default: 0
// #define ARG_TYPE_DEFAULT  default: 3
// #define ARG_TYPE_DEFAULT(x)  default: ((sizeof(x) <= sizeof(void *)) ? 0 : 3)

#define VA_ARGS_TYPE_BIT_SIZE_PRIV(P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, Pn, ...)  \
    ( (_Generic(P2,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 0) \
    | (_Generic(P3,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 2) \
    | (_Generic(P4,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 4) \
    | (_Generic(P5,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 6) \
    | (_Generic(P6,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 8) \
    | (_Generic(P7,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 10) \
    | (_Generic(P8,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 12) \
    | (_Generic(P9,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 14) \
    | (_Generic(P10, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 16) \
    | (_Generic(P11, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 18) \
    | (_Generic(P12, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 20) \
    | (_Generic(P13, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 22) \
    | (_Generic(P14, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 24) \
    | (_Generic(P15, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 26) \
    | (_Generic(P16, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 28) \
    | (_Generic(P17, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) << 30))

#define VA_ARGS_TYPE_BIT_SIZE(...)  VA_ARGS_TYPE_BIT_SIZE_PRIV(-1, ##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)


#define VA_ARGS_TYPE_CHECK_PRIV(P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, Pn, ...) \
    _Static_assert(_Generic(P2,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P3,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P4,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P5,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P6,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P7,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P8,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P9,  ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P10, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P11, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P12, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P13, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P14, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P15, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P16, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n"); \
    _Static_assert(_Generic(P17, ARG_TYPE_4BYTE, ARG_TYPE_8BYTE, ARG_TYPE_STRING, ARG_TYPE_DEFAULT) != 3, "arg type error!\n");

#define VA_ARGS_TYPE_CHECK(...)  VA_ARGS_TYPE_CHECK_PRIV(-1, ##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define DLOG_LEVEL_TO_STR(level) #level

#define dlog_printf(level, format, ...) \
    if(config_dlog_enable){ \
        VA_ARGS_NUM_CHECK(VA_ARGS_NUM(__VA_ARGS__)); \
        VA_ARGS_TYPE_CHECK(__VA_ARGS__); \
        DLOG_STR_TAB_SIZE_CHECK(); \
        __attribute__((used,section(".dlog.rodata.string."DLOG_LEVEL_TO_STR(level))))  static const char dlog_printf_str[] = format; \
        __attribute__((used,section(".dlog.rodata.str_tab."DLOG_LEVEL_TO_STR(level)))) static const struct dlog_str_tab_s dlog_printf_str_tab = \
        { \
            .arg_num = (u32)VA_ARGS_NUM(__VA_ARGS__), \
            .dlog_level = (u8)level,\
            .dlog_str_addr = (u32)dlog_printf_str, \
            .arg_type_bit_map = (u32)VA_ARGS_TYPE_BIT_SIZE(__VA_ARGS__) \
        }; \
        dlog_print((const struct dlog_str_tab_s *)&dlog_printf_str_tab, \
                ((u32)VA_ARGS_NUM(__VA_ARGS__) & 0x0F) | ((VA_ARGS_TYPE_BIT_SIZE(__VA_ARGS__) & 0x0FFFFFFF) << 4), \
                format, \
                ##__VA_ARGS__); \
    };

#if 0
#define dlog_print_args_check(...)  \
            VA_ARGS_NUM_CHECK(VA_ARGS_NUM(__VA_ARGS__)); \
            VA_ARGS_TYPE_CHECK(__VA_ARGS__); \
            DLOG_STR_TAB_SIZE_CHECK();

#define dlog_uart_log_print(format, level, args_num, args_bit_map, ...)  \
        { \
            __attribute__((used,section(".dlog.rodata.string")))  static const char dlog_printf_str[] = _LOG_TAG format; \
            __attribute__((used,section(".dlog.rodata.str_tab"))) static const struct dlog_str_tab_s dlog_printf_str_tab = \
            { \
                .arg_num = (u32)args_num, \
                .dlog_level = (u8)level,\
                .dlog_str_addr = (u32)dlog_printf_str, \
                .arg_type_bit_map = (u32)args_bit_map \
            }; \
            dlog_print((const struct dlog_str_tab_s *)&dlog_printf_str_tab, \
                    ((u32)args_num << 28) | (args_bit_map & 0x0FFFFFFF), \
                    format, \
                    ##__VA_ARGS__); \
        }

#define dlog_uart_print(format, level, args_num, args_bit_map, ...)  \
        { \
            __attribute__((used,section(".dlog.rodata.string")))  static const char dlog_printf_str[] = format; \
            __attribute__((used,section(".dlog.rodata.str_tab"))) static const struct dlog_str_tab_s dlog_printf_str_tab = \
            { \
                .arg_num = (u32)args_num, \
                .dlog_level = (u8)level,\
                .dlog_str_addr = (u32)dlog_printf_str, \
                .arg_type_bit_map = (u32)args_bit_map \
            }; \
            dlog_print((const struct dlog_str_tab_s *)&dlog_printf_str_tab, \
                    ((u32)args_num << 28) | (args_bit_map & 0x0FFFFFFF), \
                    format, \
                    ##__VA_ARGS__); \
        }

#define dlog_uart_puts(format)  \
        { \
            __attribute__((used,section(".dlog.rodata.string")))  static const char dlog_printf_str[] = "%s"; \
            __attribute__((used,section(".dlog.rodata.str_tab"))) static const struct dlog_str_tab_s dlog_printf_str_tab = \
            { \
                .arg_num = (u32)1, \
                .dlog_level = (u8)0xFE,\
                .dlog_str_addr = (u32)dlog_printf_str, \
                .arg_type_bit_map = (u32)2 \
            }; \
            dlog_print(&dlog_printf_str_tab, \
                    ((u32)dlog_printf_str_tab.arg_num << 28) | (dlog_printf_str_tab.arg_type_bit_map & 0x0FFFFFFF), "%s", format); \
        }
#endif

#endif
