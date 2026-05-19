#ifndef __HEALTH_LONG_DETECT_H__
#define __HEALTH_LONG_DETECT_H__

#include "typedef.h"
#include "app_config.h"
#include "generic/list.h"

// #define HEALTH_ALL_DAY_CHECK_ENABLE 0    // 长时间检测的宏

#define HR_DATA_SIZE                    1
#define VALID_DATA_SIZE                 5
#define HR_DATA_REPORT_HEAD_SIZE        13


//  VM存储3类信息的item_id
#define ITEM_ID_META_LIST           10  // 存储所有批次的元数据（StoreMeta数组）（每次存储的首时间戳 + 数据量）// 时间戳 + 数据量
#define ITEM_ID_HR_DATA             11  // 存储所有hr_data的拼接数据 // 数据
#define ITEM_ID_TOTAL_TIMES         12  // 存储累计存储次数 // 存储次数
#define CFG_USER_APP_ALL_DAY_DETECT_MODE     14

#define TWS_FUNC_ID_SLAVE_PUSH_ALLDAY_DATA          TWS_FUNC_ID('S', 'P', 'A', 'D')
#define TWS_FUNC_ID_SLAVE_PUSH_ALLDAY_DATA_FINISH   TWS_FUNC_ID('S', 'P', 'D', 'F')
#define TWS_FUNC_ID_MASTER_ALLDAY_DATA_REQ          TWS_FUNC_ID('M', 'A', 'D', 'R')
#define TWS_FUNC_ID_MASTER_PUSH_DATA_SUCCESS        TWS_FUNC_ID('M', 'p', 'D', 's')
#define TWS_FUNC_ID_ALLDAY_TIMESTAMP                TWS_FUNC_ID('A', 'D', 'T', 'S')

// 定义链表头   //链表节点结构体
static struct list_head l_detect_list_head = LIST_HEAD_INIT(l_detect_list_head);//初始化链表头，链表头的 prev 和 next 指针都指向自身，形成一个空链表（闭环）

// 定义链表头   //链表节点结构体
static struct list_head salve_data_list_head = LIST_HEAD_INIT(salve_data_list_head);    //

struct bulk_list {
    struct list_head head;
    u8 hr_data;
    u32 timestamp;
};

// 定义时间字段结构体（年用u16，其他用u8）
typedef struct {
    uint16_t year;   // 年（u16才能存4位数字）
    uint8_t  month;  // 月
    uint8_t  day;    // 日
    uint8_t  hour;   // 时
    uint8_t  minute; // 分
    uint8_t  second; // 秒
} TimeFields;

// 每批存储的元数据（首个时间戳 + 本次个数）
typedef struct {
    u32 first_timestamp;  // 本批数据的首个时间戳
    u16 count;            // 本批数据的个数（链表节点数）
} StoreMeta;  // 单个元数据大小：4+2=6字节

// 按天分组的心率包（存储单天完整数据+文件头）
typedef struct {
    u8 *buff;               // 单天完整包：文件头（10字节）+ 数据
    u16 len;                // 单天包总长度（文件头+数据）
    u8 year;                // 年份（固定2025）
    u8 month;               // 当月（从timestamp提取）
    u8 day;                 // 当天（从timestamp提取，分组核心）
    struct bulk_list *last_node; // 该天最后一个数据节点（用于后续删除标记）
} DayHRPackage;

// 待确认发送任务（缓存已发数据，等待APP回复）
typedef struct {
    struct list_head list;  // 链表节点（加入全局待确认列表）
    DayHRPackage *packages; // 按天分组的包数组
    u16 pkg_cnt;            // 分组数量
    u8 op_code_sn;          // 指令序号（匹配APP回复）
    u32 timeout_tick;       // 超时时间戳（避免内存泄露）
} PendingSendTask;

// 长时检测的初始化
void health_all_day_detect_init(void);

// 长时检测的开关
void health_all_day_detect_switch(u8 en);
// 获取心率预处理
void health_all_day_detect_preprocess();
// 预处理完回到这获取心率数据，填链表
void health_all_day_detect_time_callback();

/**
 * @brief 关机时将链表数据存入VM，更新元数据和总次数
 * @param head 链表头指针
 * @return 0：成功；<0：错误码
 */
int save_on_shutdown(void);
/**
 * @brief 开机时读取VM数据，还原所有hr_data及其时间戳
 * @return 0：成功；<0：错误码
 */
int restore_on_boot(void);

// 后台健康检测，数据发给app后，等app确认收到数据，再删除已发数据
void background_check_clear_data(void);

u16 ear_sports_handle_hr_data_pack(u8 **data_out);

// 时间字符转数字
// 转换函数：输入时间字符串，输出解析后的时间字段（成功返回0，失败返回-1）
int time_str_to_numbers(const char *time_str, TimeFields *numbers);

// 后续应该用RTC实时时钟替代
// 给输入时间增加5分钟，并处理进位（分钟→小时→日）
// 函数：给时间增加5分钟，并处理进位（分钟→小时→日→月→年）
void add_5minutes(TimeFields *time);

// 主机推时间戳给对耳，同时开启时间戳递进
void rtc_imitate_add_5minutes();
/**
 * @brief 根据当前时间（long_check_timestamp）清理VM中超过2天的批次数据
 */
void clear_vm_over_2days_by_current_time(void);

// 主机成功把从机的数据都推给app后，从机要从链表清掉数据
void salve_clear_all_day_data(void);

// 从机定时检测主机是否把数据推给app
void salve_check_master_push_data_finished();

// 主机检查tws从机是否传完数据
void master_check_slave_data_push_finished();

// 主机请求从机推数据
void master_get_all_day_data();
void tws_sync_health_all_day_timestamp(TimeFields *numbers);
#endif

