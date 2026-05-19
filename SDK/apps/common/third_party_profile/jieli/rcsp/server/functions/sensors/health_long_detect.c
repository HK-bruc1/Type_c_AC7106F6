#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".health_long_detect.data.bss")
#pragma data_seg(".health_long_detect.data")
#pragma const_seg(".health_long_detect.text.const")
#pragma code_seg(".health_long_detect.text")
#endif
#include "app_config.h"
#include "typedef.h"
#include "rcsp_define.h"
#include "ble_rcsp_server.h"
#include "sport_data_func.h"
#include "health_long_detect.h"
#include "JL_rcsp_protocol.h"
#include "hrSensor_manage.h"

#include "rcsp/server/functions/sensors/ear_sports_data_opt.h"
#include "timer.h"
#include "btstack/avctp_user.h"
#include "hx3011/hx3011_hrs_agc.h"
#include "hx3011/hx3011_spo2_agc.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if 1
#define LOG_TAG             "[HEALTH_LONG_CHECK]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"
#else
#define log_info(fmt, ...)
#define log_error(fmt, ...)
#define log_debug(fmt, ...)
#endif

#if HEALTH_ALL_DAY_CHECK_ENABLE

#define HEALTH_ALL_DAY_DETECT_TIMEOUT   1   // 多久检测一次，默认5分钟
#define MAX_KEEP_DAYS                   2   // 只保存两天的数据

static OS_MUTEX l_detect_mutex;     // 链表互斥量
static OS_MUTEX salve_data_mutex;   // 链表互斥量

TimeFields long_check_timestamp;    // 每次存储数据的首时间戳

static u8 health_all_day_detect_start_timer_id = 0; // 全天后台检测开始定时器
static u8 health_all_day_detect_pro_timer_id = 0;       // 预处理的定时器
static u8 health_all_day_detect_data_timer_id = 0;       // 拿数的定时器
static u8 health_all_day_sensor_cb_timer_id = 0;    // 定时给"app_sensor" post信号
static u8  rtc_imitate_cb_timer_id = 0;

extern u8 hr_detect_status;     // 心率检测的状态
extern u8 spo2_detect_status;   // 血氧检测的状态

u8 health_all_day_detect_mode = 0;                  // 全天后台检测的开关

static void sensor_data_check_cb(void *parm)
{
    os_taskq_post_type("app_sensor", (Q_MSG + 1), 0, NULL);
}

// 链表填数和取数要用互斥量
void health_all_day_detect_init(void)
{
    os_mutex_create(&l_detect_mutex);
    // os_mutex_create(&salve_data_mutex);
}

// 长时检测的开关（未完善）
void health_all_day_detect_switch(u8 en)
{
    line_inf
    if (health_all_day_detect_mode == en) {
        return ;    // 不重复开关
    }
    health_all_day_detect_mode = en;
    int ret = syscfg_write(CFG_USER_APP_ALL_DAY_DETECT_MODE, &health_all_day_detect_mode, sizeof(health_all_day_detect_mode));
    printf("ret:%d, health_all_day_detect_mode:%d\n", ret, health_all_day_detect_mode);
    if (health_all_day_detect_mode) {
        if (!health_all_day_detect_start_timer_id) {
            health_all_day_detect_start_timer_id = sys_timeout_add(NULL, health_all_day_detect_preprocess, (HEALTH_ALL_DAY_DETECT_TIMEOUT * 60000) - 30000);   // 要预留30s给算法做处理
        }
    } else  {
        // 关闭此功能，要清除定时器，要关闭3011，要清除数据；还没做
        if (health_all_day_detect_start_timer_id) {
            sys_timer_del(health_all_day_detect_start_timer_id);
            health_all_day_detect_start_timer_id = 0;
        }
        if (health_all_day_sensor_cb_timer_id) {
            sys_timer_del(health_all_day_sensor_cb_timer_id);
            health_all_day_sensor_cb_timer_id = 0;
        }
        // 正在实时检测，就不要做处理，不然会把实时监测的工作模式覆盖掉
        // 没有实时监测，才关掉
        if ((!hr_detect_status) && (!spo2_detect_status)) {
            hr_sensor_measure_hr_stop();
            hr_sensor_measure_wear_start(0, 0);
        }
    }
}

// 心率模式预处理
void health_all_day_detect_preprocess()
{
    if (health_all_day_detect_start_timer_id) {
        sys_timer_del(health_all_day_detect_start_timer_id);
        health_all_day_detect_start_timer_id = 0;
    }
    if (health_all_day_detect_pro_timer_id) {
        sys_timer_del(health_all_day_detect_pro_timer_id);
        health_all_day_detect_pro_timer_id = 0;
    }
    line_inf
    if (!health_all_day_detect_data_timer_id) {
        health_all_day_detect_data_timer_id = sys_timeout_add(NULL, health_all_day_detect_time_callback, 30000);    // 30s后拿数
    }

    // 3011先跑起来
    // hx3011_exit_low_power();
    // 非佩戴模式不做后台检测，后台拿数为0即可。
    // 血氧实时检测模式，不要开后台，防止血氧实时监测模式被覆盖掉
    // 心率实时监测模式，后台直接拿数即可，不用重复初始化心率工作模式
    if (!health_all_day_detect_mode) {
        return ;
    }
    if (!hr_sensor_get_wear_status() || spo2_detect_status || hr_detect_status) {
        return ;
    }
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role()) {
        hr_sensor_measure_hr_stop();
        return ;    // 从机不做后台检测
    }

#endif
    hr_sensor_measure_hr_start(0, 0);
    if (!health_all_day_sensor_cb_timer_id) {
        health_all_day_sensor_cb_timer_id = sys_timer_add(NULL, sensor_data_check_cb, 320);
    }
}

// 获取心率数据，预处理完回到这，填链表
void health_all_day_detect_time_callback()
{
    if (health_all_day_detect_data_timer_id) {
        sys_timer_del(health_all_day_detect_data_timer_id);
        health_all_day_detect_data_timer_id = 0;
    }
    line_inf
    if (!health_all_day_detect_pro_timer_id) {
        health_all_day_detect_pro_timer_id = sys_timeout_add(NULL, health_all_day_detect_preprocess, (HEALTH_ALL_DAY_DETECT_TIMEOUT * 60000) - 30000);   // 要预留30s给算法做处理
    }
    log_info("hr_all_day_data = %d\n", hr_sensor_get_heart_rate());

    // 拿数据，填链表
    os_mutex_pend(&l_detect_mutex, 0);
    //insert
    struct bulk_list *new_bulk;
    new_bulk = malloc(sizeof(struct bulk_list));    //动态分配也可能阻塞任务
    new_bulk->hr_data = hr_sensor_get_heart_rate();
    if (!hr_sensor_get_wear_status() || spo2_detect_status) {   // 非佩戴模式和血氧模式，直接给数据0即可。
        new_bulk->hr_data = 0;
    }
    log_info("hour:%d, minute:%d\n", long_check_timestamp.hour, long_check_timestamp.minute);
    new_bulk->timestamp = (long_check_timestamp.month << 24) | (long_check_timestamp.day << 16) | (long_check_timestamp.hour << 8) | long_check_timestamp.minute;
    list_add_tail(&new_bulk->head, &l_detect_list_head);  // 尾部插入的方式插入
    os_mutex_post(&l_detect_mutex);

    // 没有实时监测，要把佩戴模式开起来，不然实时监测的佩戴状态不对
    if ((!hr_detect_status) && (!spo2_detect_status)) {
        hr_sensor_measure_hr_stop();
        hr_sensor_measure_wear_start(0, 0);
    }

    if (health_all_day_sensor_cb_timer_id) {
        sys_timer_del(health_all_day_sensor_cb_timer_id);
        health_all_day_sensor_cb_timer_id = 0;
    }
}

#if TCFG_BT_SUPPORT_MAP
#define PROFILE_CMD_TRY_AGAIN_LATER 	    -1004
void bt_get_time_date()
{
    int error = bt_cmd_prepare(USER_CTRL_HFP_GET_PHONE_DATE_TIME, 0, NULL);
    log_info(">>>>>error = %d\n", error);
    if (error == PROFILE_CMD_TRY_AGAIN_LATER) {
        sys_timeout_add(NULL, bt_get_time_date, 100);
    }
}
void phone_date_and_time_feedback(u8 *data, u16 len)
{
    log_info("time:%s\n", data);

#if TCFG_IFLYTEK_ENABLE
    extern void get_time_from_bt(u8 * data);
    get_time_from_bt(data);
    extern void ifly_vad_demo(void);
    ifly_vad_demo();
#endif
}

void map_get_time_data(char *time, int status)
{
    log_info("func %s line %d \n", __func__, __LINE__);
    if (status == 0) {
        log_info("time:%s\n", time);
        u8 temp_health_all_day_detect_mode;
        int ret = syscfg_read(CFG_USER_APP_ALL_DAY_DETECT_MODE, &temp_health_all_day_detect_mode, sizeof(temp_health_all_day_detect_mode));    // 记录关机前的状态
        printf("%s,ret :%d, health_all_day_detect_mode:%d\n", __FUNCTION__, ret, temp_health_all_day_detect_mode);
        if (ret != sizeof(temp_health_all_day_detect_mode)) {
            temp_health_all_day_detect_mode = 0;
        }
        if (temp_health_all_day_detect_mode) {
            health_all_day_detect_switch(1);
        }
        time_str_to_numbers(time, &long_check_timestamp);
        log_info("year:%d,month:%d,day:%d,hour:%d,minute:%d,second:%d\n", long_check_timestamp.year, long_check_timestamp.month, long_check_timestamp.day, long_check_timestamp.hour, long_check_timestamp.minute, long_check_timestamp.second);
        if (!rtc_imitate_cb_timer_id) {
            rtc_imitate_cb_timer_id = sys_timer_add(NULL, rtc_imitate_add_5minutes, HEALTH_ALL_DAY_DETECT_TIMEOUT * 60000);
        }
        // clear_vm_over_2days_by_current_time();  // 检测数据时效性
    } else {
        log_info(">>>map get fail\n");
        sys_timeout_add(NULL, bt_get_time_date, 100);
    }
}
#endif


/**
 * @brief 关机时将链表数据存入VM，更新元数据和总次数
 * @param head 链表头指针
 * @return 0：成功；<0：错误码
 */
int save_on_shutdown(void)
{
    line_inf
    struct list_head *head = &l_detect_list_head;
    if (head == NULL || list_empty(head)) {
        log_info("save_on_shutdown null !!!\n");
        return 0;
    }

    // --------------------------
    // 步骤1：收集本批数据的关键信息
    // --------------------------
    // 1.1 获取本批首个时间戳（第一个节点的timestamp）
    struct bulk_list *first_node = list_first_entry(head, struct bulk_list, head);
    u32 first_ts = first_node->timestamp;

    // 1.2 统计本批数据个数（链表节点数）
    uint16_t current_count = 0;
    struct bulk_list *p;
    list_for_each_entry(p, head, head) {
        current_count++;
    }

    // 1.3 收集本批所有hr_data
    u8 *current_hr_data = malloc(current_count * sizeof(u8));
    uint16_t i = 0;
    list_for_each_entry(p, head, head) {
        current_hr_data[i] = p->hr_data;
        log_info("p->timestamp:%x", p->timestamp);
        log_info("p->hr_data:%d", p->hr_data);
        i++;
    }

    // --------------------------
    // 步骤2：读取VM中已有的累计次数
    // --------------------------
    uint32_t old_total = 0;  // 旧的总次数
    int ret = syscfg_read(ITEM_ID_TOTAL_TIMES, &old_total, sizeof(old_total));
    log_info("ret:%d, old_total:%d ", ret, old_total);
    uint32_t new_total = old_total + 1;  // 新总次数（本次存储算1次）

    // --------------------------
    // 步骤3：更新元数据(时间戳+数据量)
    // --------------------------
    // 3.1 读取已有元数据(时间戳+数据量)
    uint32_t old_meta_len = old_total * sizeof(StoreMeta);  // 旧元数据总长度
    StoreMeta *old_meta = NULL;
    if (old_total > 0) {
        old_meta = malloc(old_meta_len);
        memset(old_meta, 0, old_meta_len);
        ret = syscfg_read(ITEM_ID_META_LIST, old_meta, old_meta_len);
    }

    // 3.2 拼接旧元数据和新元数据（(时间戳+数据量)）
    uint32_t new_meta_len = new_total * sizeof(StoreMeta);
    StoreMeta *new_meta = malloc(new_meta_len);
    // 复制旧元数据(时间戳+数据量)
    if (old_meta != NULL && new_meta != NULL && old_total > 0) {
        memset(new_meta, 0, new_meta_len);  // 初始化
        memcpy(new_meta, old_meta, old_meta_len);
    } else {
        return 0;
    }
    // 追加新元数据（本批）
    new_meta[old_total].first_timestamp = first_ts;  // 赋值本批首个时间戳
    new_meta[old_total].count = current_count;        // 赋值本批数据个数

    // 3.3 写入新元数据(时间戳+数据量)到VM
    ret = syscfg_write(ITEM_ID_META_LIST, new_meta, new_meta_len);
    free(new_meta);

    // --------------------------
    // 步骤4：更新hr_data（追加本批数据）
    // --------------------------
    // 4.1 读取已有hr_data
    // uint32_t old_hr_len = old_total == 0 ? 0 : (old_meta_len / sizeof(StoreMeta)) * sizeof(u8);  // 旧hr_data总长度（字节）
    uint32_t old_hr_len = 0;
    if (old_total > 0) {
        // 遍历旧meta，累加所有count得到旧hr_data总长度
        for (uint32_t i = 0; i < old_total; i++) {
            // 若count为0，视为无效，用第一次存储的默认逻辑（假设第一次count≥1）
            if (old_meta[i].count == 0) {
                old_hr_len += 0;  // 至少按1条数据处理，避免0长度
            } else {
                old_hr_len += old_meta[i].count;
            }
        }
        free(old_meta);
    }
    // 更准确的计算：遍历旧meta累加count×sizeof(u8)
    // 简化版：假设old_hr_len = 累计count×sizeof(u8)，这里用旧总次数对应的累计count
    // （实际应遍历old_meta计算，此处简化）
    uint8_t *old_hr_data = NULL;
    if (old_hr_len > 0) {
        old_hr_data = malloc(old_hr_len);
        memset(old_hr_data, 0, old_hr_len);  // 初始化
        ret = syscfg_read(ITEM_ID_HR_DATA, old_hr_data, old_hr_len);
    }

    // 4.2 拼接旧hr_data和新hr_data
    // 步骤4.2 拼接时，强制确认旧数据长度和新数据长度
    uint32_t current_hr_len = current_count;  // u8占1字节，直接等于current_count
    uint32_t new_hr_len = old_hr_len + current_hr_len;
    uint8_t *new_hr_data = malloc(new_hr_len);
    // 手动清零新缓冲区，避免未初始化的垃圾值
    memset(new_hr_data, 0, new_hr_len);

    // 复制旧数据（即使old_hr_len计算有误，至少前old_hr_len字节是旧数据）
    if (old_hr_len > 0 && old_hr_data != NULL) {
        memcpy(new_hr_data, old_hr_data, old_hr_len);
        free(old_hr_data);
    }
    // 复制新数据（强制从old_hr_len位置开始，避免覆盖旧数据）
    memcpy(new_hr_data + old_hr_len, current_hr_data, current_hr_len);
    free(current_hr_data);

    // 4.3 写入新hr_data到VM
    ret = syscfg_write(ITEM_ID_HR_DATA, new_hr_data, new_hr_len);
    free(new_hr_data);

    // --------------------------
    // 步骤5：更新累计存储次数
    // --------------------------
    ret = syscfg_write(ITEM_ID_TOTAL_TIMES, &new_total, sizeof(new_total));

    // --------------------------
    // 步骤6：清空链表（避免重复存储）
    // --------------------------
    struct bulk_list *n;
    list_for_each_entry_safe(p, n, head, head) {
        list_del(&p->head);
        free(p);
    }

    printf("new_total:%d,first_ts=0x%04X,current_count=%d\n", new_total, first_ts, current_count);
    return 0;
}

/**
 * @brief 开机时读取VM数据，还原所有hr_data及其时间戳
 * @return 0：成功；<0：错误码
 */
int restore_on_boot(void)
{
    line_inf

    // --------------------------
    // 步骤1：读取累计存储次数
    // --------------------------
    uint32_t total_times = 0;
    int ret = syscfg_read(ITEM_ID_TOTAL_TIMES, &total_times, sizeof(total_times));

    printf("total_times=%d\n", total_times);

    // --------------------------
    // 步骤2：读取所有元数据（StoreMeta数组）
    // --------------------------
    uint32_t meta_len = total_times * sizeof(StoreMeta);
    StoreMeta *meta_list = malloc(meta_len);

    ret = syscfg_read(ITEM_ID_META_LIST, meta_list, meta_len);


    // --------------------------
    // 步骤3：读取所有hr_data
    // --------------------------
    // 计算总hr_data长度（遍历meta累加count×sizeof(u8)）
    uint32_t total_hr_count = 0;
    for (uint32_t i = 0; i < total_times; i++) {
        total_hr_count += meta_list[i].count;
    }
    uint32_t hr_data_len = total_hr_count * sizeof(u8);
    u8 *all_hr_data = malloc(hr_data_len);

    ret = syscfg_read(ITEM_ID_HR_DATA, all_hr_data, hr_data_len);

    // --------------------------
    // 步骤4：还原每条数据的时间戳
    // --------------------------
    uint32_t hr_index = 0;  // 用于遍历all_hr_data
    struct bulk_list *new_bulk;
    for (uint32_t batch = 0; batch < total_times; batch++) {
        StoreMeta meta = meta_list[batch];
        TimeFields current_ts; // 定义当前时间戳
        uint16_t count = meta.count;               // 本批数据个数
        // 提取月份（占据第24-31位）
        current_ts.month = (meta.first_timestamp >> 24) & 0xFF;
        // 提取日期（占据第16-23位）
        current_ts.day = (meta.first_timestamp >> 16) & 0xFF;
        // 提取小时（占据第8-15位）
        current_ts.hour = (meta.first_timestamp >> 8) & 0xFF;
        // 提取分钟（占据第0-7位，无需右移）
        current_ts.minute = meta.first_timestamp & 0xFF;
        printf("\nbatch:%d,first_ts=0x%04X,count:%d\n", batch + 1, meta.first_timestamp, count);
        for (uint16_t i = 0; i < count; i++) {
            // 计算第i条数据的时间戳：首个时间 + 5分钟×i（5分钟=300秒，这里按timestamp格式累加）
            // 假设timestamp是"时<<8 | 分"，则5分钟累加逻辑需转换为时分计算（参考之前的add_5minutes）

            // for循环获取，每5分钟后的时间戳
            add_5minutes(&current_ts);
            new_bulk = malloc(sizeof(struct bulk_list));    //动态分配也可能阻塞任务
            new_bulk->hr_data = all_hr_data[hr_index];
            new_bulk->timestamp = (current_ts.month << 24) | (current_ts.day << 16) | (current_ts.hour << 8) | (current_ts.minute);
            list_add_tail(&new_bulk->head, &l_detect_list_head);  // 尾部插入的方式插入
            // 输出该条数据
            printf("  i:%d,current_ts=0x%04X,all_hr_data[hr_index]=%d\n", i + 1, new_bulk->timestamp, new_bulk->hr_data);
            hr_index++;
        }
    }
    // 读完后清空数据
    total_times = 0;                        // 数值型直接置0
    memset(meta_list, 0xFF, meta_len);         // 数组型用0填充（按实际长度）
    memset(all_hr_data, 0xFF, hr_data_len);    // 数组型用0填充（按实际长度）
    ret = syscfg_write(ITEM_ID_TOTAL_TIMES, &total_times, sizeof(total_times));
    ret = syscfg_write(ITEM_ID_META_LIST, meta_list, meta_len);
    ret = syscfg_write(ITEM_ID_HR_DATA, all_hr_data, hr_data_len);
    // 释放资源
    free(meta_list);
    free(all_hr_data);
    return 0;
}

u16 l_detect_list_len = 0;
static u32 last_timestamp = 0;

u16 ear_sports_handle_hr_data_pack(u8 **data_out)
{
    u16 protocol_head_len = 0;
    u16 total_pack_len = 0;
    static u8 salve_list_len = 0;
    bool is_slave_have_data = 0;
    // os_mutex_pend(&l_detect_mutex, 0);
    *data_out = NULL;
    struct bulk_list *p = NULL;
    // 1. 统计链表总节点数，避免空链表操作
    list_for_each_entry(p, &l_detect_list_head, head) {
        l_detect_list_len++;
    }
#if TCFG_USER_TWS_ENABLE
    // os_mutex_pend(&salve_data_mutex, 0);
    struct list_head *head = &salve_data_list_head;
    if (head == NULL || list_empty(head)) {
        log_info("salve_data_list_head null !!!\n");
        is_slave_have_data = 0;
    } else {
        is_slave_have_data = 1;
        list_for_each_entry(p, &salve_data_list_head, head) {
            salve_list_len++;
        }
        l_detect_list_len = l_detect_list_len + salve_list_len;
    }
#endif

    printf("l_detect_list_len:%d", l_detect_list_len);  // 总数据长度
    if (l_detect_list_len == 0) {
        // log_info("No HR data to pack");
        // log_info("p-:0x%x\n", p->timestamp);
        // log_info("p-:%d\n", p->hr_data);
        // os_mutex_post(&l_detect_mutex);
        // os_mutex_post(&salve_data_mutex);
        return 0;
    }

    // 2. 先统计有多少个不同的天（确定要分配的包数）
    u16 pkg_cnt = 0;
    u8 last_month = 0;
    u8 last_day = 0;
    u8 day_data_len[256] = {0};
    u16 offset = 0;
    u16 curr_data_cnt = 0;
    u8 node_month = 0;
    u8 node_day = 0;
    u8 node_hour = 0;
    u8 node_min = 0;
    pkg_cnt = 1;
    u8 *hr_data_buff = zalloc(4 + 13 * 3 + l_detect_list_len * 5);

    *data_out = hr_data_buff;

    bool first_day_data_flag = 0;
    offset = offset + 17;   // 4（总的协议头） + 13（小文件协议头）
    list_for_each_entry(p, &l_detect_list_head, head) {
        node_month = (p->timestamp >> 24) & 0xFF;
        node_day = (p->timestamp >> 16) & 0xFF;
        node_hour = (p->timestamp >> 8) & 0xFF;  // 时（1字节）
        node_min = p->timestamp & 0xFF;          // 分（1字节）
        if (!first_day_data_flag) {
            last_month = node_month;
            last_day = node_day;
            first_day_data_flag = 1;
        }
        // // 跨天：补充上一天的
        if (node_month != last_month || node_day != last_day) {
            last_month = node_month;
            last_day = node_day;
            day_data_len[pkg_cnt - 1] = curr_data_cnt;
            pkg_cnt ++;
            curr_data_cnt = 0;
            offset = offset + 13;   // 预留补充小文件协议头
        }
        // log_info("p-:0x%x\n", p->timestamp);
        // log_info("p-:%d\n", p->hr_data);
        hr_data_buff[offset++] = (p->timestamp >> 8) & 0xff;
        hr_data_buff[offset++] = p->timestamp & 0xff;
        hr_data_buff[offset++] = HR_DATA_SIZE >> 8;
        hr_data_buff[offset++] = HR_DATA_SIZE & 0xff;
        hr_data_buff[offset++] = p->hr_data;
        curr_data_cnt++;
        // log_info("curr_data_cnt:%d\n", curr_data_cnt);
        last_timestamp = p->timestamp;      // 记录最后一个时间戳，用于删除数据
        // printf("last_timestamp:%x\n", last_timestamp);
    }
    day_data_len[pkg_cnt - 1] = curr_data_cnt;  // 补充最后一次的数据长度。
    curr_data_cnt = 0;
    first_day_data_flag = 0;

#if TCFG_USER_TWS_ENABLE
    if (is_slave_have_data) {
        bool salve_first_day_data_flag = 0;
        offset = offset + 13;   // 从机数据不用补充总的协议头了
        list_for_each_entry(p, &salve_data_list_head, head) {
            node_month = (p->timestamp >> 24) & 0xFF;
            node_day = (p->timestamp >> 16) & 0xFF;
            node_hour = (p->timestamp >> 8) & 0xFF;  // 时（1字节）
            node_min = p->timestamp & 0xFF;          // 分（1字节）
            if (!salve_first_day_data_flag) {
                last_month = node_month;
                last_day = node_day;
                salve_first_day_data_flag = 1;
            }
            // // 跨天：补充上一天的
            if (node_month != last_month || node_day != last_day) {
                last_month = node_month;
                last_day = node_day;
                day_data_len[pkg_cnt - 1] = curr_data_cnt;
                pkg_cnt ++;
                curr_data_cnt = 0;
                offset = offset + 13;   // 预留补充小文件协议头
            }
            // log_info("p-:0x%x\n", p->timestamp);
            // log_info("p-:%d\n", p->hr_data);
            hr_data_buff[offset++] = (p->timestamp >> 8) & 0xff;
            hr_data_buff[offset++] = p->timestamp & 0xff;
            hr_data_buff[offset++] = HR_DATA_SIZE >> 8;
            hr_data_buff[offset++] = HR_DATA_SIZE & 0xff;
            hr_data_buff[offset++] = p->hr_data;
            curr_data_cnt++;
            // log_info("curr_data_cnt:%d\n", curr_data_cnt);
            // printf("last_timestamp:%x\n", last_timestamp);
        }
        day_data_len[pkg_cnt - 1] = curr_data_cnt;  // 补充最后一次的数据长度。
        curr_data_cnt = 0;
        salve_first_day_data_flag = 0;
    }
#endif

    u16 next_day_head ;
    next_day_head = 0;
    // 要补充协议头
    printf("pkg_cnt:%d\n", pkg_cnt);
    for (int i = 0; i < pkg_cnt; i++) {
        // 没有跨天，要补充协议头
        // printf("day_data_len[i]:%d\n", day_data_len[i]);
        hr_data_buff[i * 13 + next_day_head + 4] = (day_data_len[i] * VALID_DATA_SIZE + 11) >> 8;    // 一天的数据长度。。
        hr_data_buff[i * 13 + next_day_head + 5] = (day_data_len[i] * VALID_DATA_SIZE + 11) & 0xff;  // 一天的数据长度。。
        hr_data_buff[i * 13 + next_day_head + 6] = 0x03;         //数据类型：全天心率数据
        hr_data_buff[i * 13 + next_day_head + 7] = (2025 >> 8) & 0xFF;    // 年高八位
        hr_data_buff[i * 13 + next_day_head + 8] = 2025 & 0xff;           // 年低八位
        hr_data_buff[i * 13 + next_day_head + 9] = node_month;       // 月
        hr_data_buff[i * 13 + next_day_head + 10] = node_day;         // 日
        hr_data_buff[i * 13 + next_day_head + 11] = 0x00;
        hr_data_buff[i * 13 + next_day_head + 12] = 0x00;
        hr_data_buff[i * 13 + next_day_head + 13] = 0x00;
        hr_data_buff[i * 13 + next_day_head + 14] = HEALTH_ALL_DAY_DETECT_TIMEOUT; // 时间间隔
        hr_data_buff[i * 13 + next_day_head + 15] = 0x00;
        hr_data_buff[i * 13 + next_day_head + 16] = 0x00;
        next_day_head += day_data_len[i] * VALID_DATA_SIZE;
    }
    // os_mutex_post(&l_detect_mutex);
    // os_mutex_post(&salve_data_mutex);

    // if(l_detect_list_len * VALID_DATA_SIZE + HR_DATA_REPORT_HEAD_SIZE != offset){
    //     printf("=====l_detect_list_len err=====");
    //     return 0;
    // }
    // printf("ear_sports_handle_hr_data_pack:%d",offset);
    // put_buf(hr_data_buff,offset);
    // printf("data_out:%x",hr_data_buff);
    protocol_head_len = 2 + 13 * pkg_cnt + next_day_head;   // 协议定义有效数据长度（LTV）
    total_pack_len = 4 + 13 * pkg_cnt + next_day_head;      // 所有数据长度
    // printf("next_day_head:%d\n", next_day_head);
    printf("total_pack_len:%d\n", total_pack_len);
    hr_data_buff[0] = (protocol_head_len >> 8) & 0xFF;
    hr_data_buff[1] = protocol_head_len & 0xFF;
    hr_data_buff[2] = 0x09;
    hr_data_buff[3] = 0x20;
    // put_buf(hr_data_buff, total_pack_len);
    offset = 0;
    return total_pack_len;
}

// 后台健康检测，数据发给app后，等app确认收到数据，再删除已发数据
void background_check_clear_data(void)
{
    line_inf
    os_mutex_pend(&l_detect_mutex, 0);
    if (last_timestamp == 0) {
        os_mutex_post(&l_detect_mutex);
        return;
    }
    struct bulk_list *p = NULL;
    struct bulk_list *n = NULL; // 用于保存下一个节点
    list_for_each_entry_safe(p, n, &l_detect_list_head, head) {
        if (last_timestamp == p->timestamp) {
            last_timestamp = 0;
            list_del(&p->head);
            free(p);
            os_mutex_post(&l_detect_mutex);
            return;
        }
        // log_info("p-:0x%x\n", p->timestamp);
        // log_info("p-:%d\n", p->hr_data);
        list_del(&p->head);
        free(p);
    }
    os_mutex_post(&l_detect_mutex);
    u8 data = 0x01;
#if TCFG_USER_TWS_ENABLE
    tws_api_send_data_to_slave(&data, sizeof(data), TWS_FUNC_ID_MASTER_PUSH_DATA_SUCCESS);  // 主机清完告诉从机清
#endif
}

// 时间字符转数字
// 转换函数：输入时间字符串，输出解析后的时间字段（成功返回0，失败返回-1）
int time_str_to_numbers(const char *time_str, TimeFields *numbers)
{
    // 检查输入合法性
    if (time_str == NULL || numbers == NULL) {
        printf("null pointer\n");
        return -1;
    }
    // 检查字符串长度（至少需要"YYYYMMDDTHHMMSS"共15个字符，如"20251022T201505"）
    if (strlen(time_str) < 15) {
        printf("Insufficient length\n");
        return -1;
    }
    // 检查分隔符'T'是否存在
    if (time_str[8] != 'T') {
        printf("Incorrect format\n");
        return -1;
    }

    // 用sscanf提取各字段（格式：YYYYMMDDTHHMMSS）
    int y, m, d, h, min, s;
    int ret = sscanf(time_str, "%4d%2d%2dT%2d%2d%2d", &y, &m, &d, &h, &min, &s);
    if (ret != 6) {  // 必须成功提取6个字段
        printf("Data error\n");
        return -1;
    }

    // 赋值给输出结构体（强制转换为对应类型）
    numbers->year = (uint16_t)y;
    numbers->month = (uint8_t)m;
    numbers->day = (uint8_t)d;
    numbers->hour = (uint8_t)h;
    numbers->minute = (uint8_t)min;
    numbers->second = (uint8_t)s;

    return 0;
}

// 给输入时间增加5分钟，并处理进位（分钟→小时→日）
// 函数：给时间增加5分钟，并处理进位（分钟→小时→日→月→年）
void add_5minutes(TimeFields *time)
{
    if (time == NULL) {
        return;
    }

    // 基础：增加5分钟
    time->minute += HEALTH_ALL_DAY_DETECT_TIMEOUT;

    // 处理分钟进位（60分钟=1小时）
    if (time->minute >= 60) {
        time->hour += time->minute / 60;
        time->minute %= 60;
    }

    // 处理小时进位（24小时=1天）
    if (time->hour >= 24) {
        time->day += time->hour / 24;
        time->hour %= 24;
    }

    // 简化处理：每月最大天数（实际需根据月份和闰年调整，这里简化为31天）
    // 可扩展：添加月份天数表（如1月31天，2月28/29天等）
    const uint8_t max_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t current_max_day = max_days[time->month];
    // 处理闰年2月（简单判断：能被4整除且不能被100整除，或能被400整除）
    if (time->month == 2) {
        if ((time->year % 4 == 0 && time->year % 100 != 0) || (time->year % 400 == 0)) {
            current_max_day = 29;
        }
    }
    // 关键优化：显式检查current_max_day是否为0（理论上不会发生，仅作容错）
    if (current_max_day == 0) {
        // 异常处理：可根据实际需求选择忽略、报错或默认赋值（如按30天处理）
        current_max_day = 30; // 临时兜底，避免除零
    }
    // 处理日进位（超过当月最大天数→进月）
    if (time->day > current_max_day) {
        time->month += time->day / current_max_day;
        time->day %= current_max_day;
        // 若整除（如31日→31/31=1，余数0→需修正为当月最后一天）
        if (time->day == 0) {
            time->day = current_max_day;
            time->month -= 1;
        }
    }

    // 处理月进位（12月→次年1月）
    if (time->month > 12) {
        time->year += time->month / 12;
        time->month %= 12;
        if (time->month == 0) {
            time->month = 12;    // 修正12的倍数情况
        }
    }
}

/**
 * @brief 根据当前时间（long_check_timestamp）清理VM中超过2天的批次数据
 */
void clear_vm_over_2days_by_current_time(void)
{
    log_info("===clear_vm_over_2days_by_current_time===\n");
    const uint8_t max_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; // 月份天数表

    // --------------------------
    // 步骤1：获取当前时间（年/月/日）
    // --------------------------
    TimeFields current = long_check_timestamp;
    uint8_t curr_month = current.month;
    uint8_t curr_day = current.day;
    uint16_t curr_year = current.year;
    log_info("Current date: %d-%02d-%02d\n", curr_year, curr_month, curr_day);

    // --------------------------
    // 步骤2：读取VM元数据和总批次
    // --------------------------
    uint32_t total_batches = 0;
    int ret = syscfg_read(ITEM_ID_TOTAL_TIMES, &total_batches, sizeof(total_batches));
    if (ret < 0 || total_batches == 0) {
        log_info("No VM data to clear\n");
        return;
    }

    uint32_t meta_len = total_batches * sizeof(StoreMeta);
    StoreMeta *meta_list = malloc(meta_len);
    if (meta_list == NULL) {
        log_error("Malloc meta_list failed\n");
        return;
    }
    ret = syscfg_read(ITEM_ID_META_LIST, meta_list, meta_len);
    if (ret < 0) {
        log_error("Read VM meta failed\n");
        free(meta_list);
        return;
    }

    // --------------------------
    // 步骤3：遍历批次，计算与当前时间的天数差
    // --------------------------
    uint32_t valid_start_idx = 0;  // 有效批次起始索引（之前的批次过期）
    uint32_t valid_total_records = 0;  // 有效数据总条数

    for (uint32_t i = 0; i < total_batches; i++) {
        StoreMeta *batch = &meta_list[i];
        if (batch->count == 0) {
            continue;
        }

        // 解析批次首个时间戳（格式：月<<24 | 日<<16 | 时<<8 | 分）
        uint8_t batch_month = (batch->first_timestamp >> 24) & 0xFF;
        uint8_t batch_day = (batch->first_timestamp >> 16) & 0xFF;

        // 计算天数差（核心：只关注月和日，简化年份影响）
        int day_diff = 0;
        if (batch_month == curr_month) {
            // 同月：直接减日
            day_diff = curr_day - batch_day;
        } else {
            // 跨月：当前月已过天数 + 批次月剩余天数
            // 例：当前3月2日，批次2月28日 → 2 + (28-28) = 2天
            uint8_t batch_month_days = max_days[batch_month];
            // 处理闰年2月
            if (batch_month == 2) {
                if ((curr_year % 4 == 0 && curr_year % 100 != 0) || (curr_year % 400 == 0)) {
                    batch_month_days = 29;
                }
            }
            day_diff = curr_day + (batch_month_days - batch_day);
        }

        // 天数差 > 2 → 该批次及之前的都过期
        if (day_diff > MAX_KEEP_DAYS) {
            valid_start_idx = i;
            break;
        }

        valid_total_records += batch->count;  // 累加有效数据
    }

    // --------------------------
    // 步骤4：清理过期数据（若有）
    // --------------------------
    uint32_t valid_batches = total_batches - valid_start_idx;
    if (valid_batches == total_batches) {
        // 无过期数据，直接返回
        log_info("No data over 2 days, skip clear\n");
        free(meta_list);
        return;
    }

    log_info("Expired batches: %d, Valid batches: %d\n", valid_start_idx, valid_batches);

    // 4.1 构建有效元数据
    uint32_t valid_meta_len = valid_batches * sizeof(StoreMeta);
    StoreMeta *valid_meta = malloc(valid_meta_len);
    if (valid_meta == NULL) {
        log_error("Malloc valid_meta failed\n");
        free(meta_list);
        return;
    }
    memcpy(valid_meta, &meta_list[valid_start_idx], valid_meta_len);

    // 4.2 构建有效HR数据
    uint32_t valid_hr_len = valid_total_records * sizeof(u8);
    u8 *valid_hr_data = malloc(valid_hr_len);
    if (valid_hr_data == NULL) {
        log_error("Malloc valid_hr_data failed\n");
        free(valid_meta);
        free(meta_list);
        return;
    }

    // 读取旧HR数据，跳过过期部分
    uint32_t old_hr_total_len = 0;
    for (uint32_t i = 0; i < total_batches; i++) {
        old_hr_total_len += meta_list[i].count;
    }
    u8 *old_hr_data = malloc(old_hr_total_len);
    if (old_hr_data == NULL) {
        log_error("Malloc old_hr_data failed\n");
        free(valid_hr_data);
        free(valid_meta);
        free(meta_list);
        return;
    }
    ret = syscfg_read(ITEM_ID_HR_DATA, old_hr_data, old_hr_total_len);
    if (ret < 0) {
        log_error("Read old HR data failed\n");
        free(old_hr_data);
        free(valid_hr_data);
        free(valid_meta);
        free(meta_list);
        return;
    }

    // 计算过期HR数据长度，复制有效部分
    uint32_t expired_hr_len = 0;
    for (uint32_t i = 0; i < valid_start_idx; i++) {
        expired_hr_len += meta_list[i].count;
    }
    memcpy(valid_hr_data, old_hr_data + expired_hr_len, valid_hr_len);

    // 4.3 写入有效数据到VM
    ret = syscfg_write(ITEM_ID_META_LIST, valid_meta, valid_meta_len);
    ret |= syscfg_write(ITEM_ID_HR_DATA, valid_hr_data, valid_hr_len);
    ret |= syscfg_write(ITEM_ID_TOTAL_TIMES, &valid_batches, sizeof(valid_batches));

    if (ret < 0) {
        log_error("Write valid VM data failed\n");
    } else {
        log_info("VM clear success: %d batches left\n", valid_batches);
    }

    // 释放内存
    free(old_hr_data);
    free(valid_hr_data);
    free(valid_meta);
    free(meta_list);
}

#if TCFG_USER_TWS_ENABLE

static u32 salve_last_timestamp = 0;
bool tws_salve_push_all_day_data_finish_flag = 0;
static bool tws_master_push_all_day_data_finish_flag = 0;

static u8 check_master_push_data_finished_cb_time_id = 0;   // 定时检查主机是否已经把从机数据都推给app
static u8 check_slave_data_push_finished_cb_time_id = 0;    // 定时检查从机是否已经把数据都推给主机

extern int ear_sports_handle_hr_data_send();

// 主机成功把从机的数据都推给app后，从机要从链表清掉数据
void salve_clear_all_day_data(void)
{
    line_inf
    if (salve_last_timestamp == 0) {
        return;
    }
    struct bulk_list *p = NULL;
    struct bulk_list *n = NULL; // 用于保存下一个节点
    list_for_each_entry_safe(p, n, &l_detect_list_head, head) {
        if (salve_last_timestamp == p->timestamp) {
            salve_last_timestamp = 0;
            list_del(&p->head);
            free(p);
            return;
        }
        list_del(&p->head);
        free(p);
    }
}

// 主机收到从机的数据，开始填链表
static void master_get_all_day_data_from_salve(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    static u8 salve_list_len = 0;
    static u16 buf_idx = 0;                    // 缓冲区当前写入索引
    uint8_t temp_idx;
    // printf("rx:%d, len:%d\n", rx, len);
    salve_list_len = len / 5;
    if (rx) {
        // 拿数据，填链表
        //insert
        struct bulk_list *new_bulk;
        for (int i = 0; i < salve_list_len; i++) {
            new_bulk = malloc(sizeof(struct bulk_list));    //动态分配也可能阻塞任务
            // new_bulk->timestamp = (data[buf_idx++] << 24) | (data[buf_idx++] << 16) | (data[buf_idx++]) << 8 | data[buf_idx++];
            temp_idx = buf_idx;
            new_bulk->timestamp = (data[temp_idx] << 24) | (data[temp_idx + 1] << 16) | (data[temp_idx + 2] << 8) | data[temp_idx + 3];
            buf_idx += 4;  // 一次性递增4，等价原逻辑但顺序明确
            new_bulk->hr_data = data[buf_idx++];
            if (new_bulk->hr_data == 0) {
                break;
            }
            list_add_tail(&new_bulk->head, &salve_data_list_head);  // 尾部插入的方式插入
        }
        printf("salve_list_len:%d, buf_idx:%d\n", salve_list_len, buf_idx);
        buf_idx = 0;
        salve_list_len = 0;

    }
}

REGISTER_TWS_FUNC_STUB(get_salve_data_sync_stub) = {
    .func_id = TWS_FUNC_ID_SLAVE_PUSH_ALLDAY_DATA,
    .func    = master_get_all_day_data_from_salve,
};

// 从机把数据都推给主机了，告诉主机，可以上传给app了
static void master_has_pushed_data_to_app(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    // printf("rx:%d, data[0]:0x%x\n", rx, data[0]);
    if (rx) {
        // 主机已经传完数据了，可以删除数据了
        // tws_master_push_all_day_data_finish_flag = 1;
        salve_clear_all_day_data();
        salve_last_timestamp = 0;
        if (check_master_push_data_finished_cb_time_id) {
            sys_timer_del(check_master_push_data_finished_cb_time_id);
            check_master_push_data_finished_cb_time_id = 0;
        }
    }
}

REGISTER_TWS_FUNC_STUB(master_app_sync_stub) = {
    .func_id = TWS_FUNC_ID_MASTER_PUSH_DATA_SUCCESS,
    .func    = master_has_pushed_data_to_app,
};

// 从机给主机推数据
static void tws_salve_push_all_day_data(void *_data, u16 len, bool rx)
{
    u16 l_detect_slave_list_len = 0;    // 链表节点总数
    u16 l_detect_slave_buf_len = 0;     // 总数据缓冲区长度（每个节点5字节：月+日+时+分+心率）
    u8 *l_detect_slave_buf = NULL;      // 总数据缓冲区：存储所有节点的拼接数据，月 + 日 + 时 + 分 + 心率数据
    u16 push_buf_cnt = 0;               // 分包总数
    u16 curr_pack_len = 0;              // 当前分包长度
    u16 curr_offset = 0;                // 当前分包在总缓冲区的偏移量
    static u16 buf_idx = 0;                    // 缓冲区当前写入索引
    u8 *data = (u8 *)_data;
    // printf("rx:%d, data[0]:0x%x\n", rx, data[0]);
    if (rx) {
        // 1.从机是否有数据
        struct bulk_list *p = NULL;
        list_for_each_entry(p, &l_detect_list_head, head) {
            l_detect_slave_list_len++;
        }
        // printf("l_detect_slave_list_len:%d",l_detect_slave_list_len);   // 总数据长度
        if (l_detect_slave_list_len == 0) {
            return ;      // 从机没有数据
        }
        // 2.遍历链表拿数据
        l_detect_slave_buf_len = l_detect_slave_list_len * 5;
        l_detect_slave_buf = zalloc(l_detect_slave_list_len * 5);
        list_for_each_entry(p, &l_detect_list_head, head) {
            l_detect_slave_buf[buf_idx++] = (p->timestamp >> 24) & 0xFF;       // 月（1字节）
            l_detect_slave_buf[buf_idx++] = (p->timestamp >> 16) & 0xFF;   // 日（1字节）
            l_detect_slave_buf[buf_idx++] = (p->timestamp >> 8) & 0xFF;    // 时（1字节）
            l_detect_slave_buf[buf_idx++] = p->timestamp & 0xFF;           // 分（1字节）
            l_detect_slave_buf[buf_idx++] = p->hr_data;                    // 心率（1字节）
            // tws从机数据链表时间戳，用于从机推送完数据后的清除
            salve_last_timestamp = p->timestamp;
            // printf("buf_idx:%d, salve->timestamp:%d, salve->hr_data:%d\n", buf_idx, p->timestamp, p->hr_data);
        }
        // // 验证：写入长度是否与预期一致（避免链表数据异常）
        // if (buf_idx != l_detect_slave_buf_len) {
        //     printf("HR data copy error: expected len=%d, actual=%d", l_detect_slave_buf_len, buf_idx);
        // }
        // 4.推数据给主机
        // tws 数据发送函数, 要求 len <= 512

        // 数据量超过512，要分包发
        push_buf_cnt = (l_detect_slave_buf_len + 509) / 510;  // 避免整除时多算一包
        for (u8 i = 0; i < push_buf_cnt; i++) {
            curr_offset = i * 510;  // 当前分包在总缓冲区的起始偏移
            // 计算当前分包长度：最后一包取剩余长度，其他包取510字节
            curr_pack_len = (i == push_buf_cnt - 1) ? (l_detect_slave_buf_len - curr_offset) : 510;
            printf("salve_curr_pack_len=%d\n", curr_pack_len);
            put_buf(&l_detect_slave_buf[curr_offset], curr_pack_len);
            tws_api_send_data_to_sibling(&l_detect_slave_buf[curr_offset], curr_pack_len, TWS_FUNC_ID_SLAVE_PUSH_ALLDAY_DATA);
        }
        buf_idx = 0;
        curr_offset = 0;
        curr_pack_len = 0;
        free(l_detect_slave_buf);
    }
}

REGISTER_TWS_FUNC_STUB(master_req_data_sync_stub) = {
    .func_id = TWS_FUNC_ID_MASTER_ALLDAY_DATA_REQ,
    .func    = tws_salve_push_all_day_data,
};

// 主机检查tws从机是否传完数据
void master_check_slave_data_push_finished()
{
    static u8 check_time;
    // if (tws_salve_push_all_day_data_finish_flag == 1) {
    //     // 从机推送数据完成，把从机的数据推给app
    //     // u8 data = 0x01;
    //     // tws_api_send_data_to_slave(&data, sizeof(data), TWS_FUNC_ID_MASTER_PUSH_DATA_SUCCESS);
    //     // tws_salve_push_all_day_data_finish_flag = 0;
    //     goto MASTER_CLEAR_DATA_AND_TIMER; // 跳转到清理逻辑
    // } else {
    check_time++;
    if (check_time >= 10) {      // 10s还没拿到数据，就认为传输失败了
        check_time = 0;
        goto MASTER_CLEAR_DATA_AND_TIMER; // 超时跳转到清理逻辑
    }
    // }

    return; // 正常情况直接返回

MASTER_CLEAR_DATA_AND_TIMER:
    if (check_slave_data_push_finished_cb_time_id) {
        sys_timer_del(check_slave_data_push_finished_cb_time_id);
        check_slave_data_push_finished_cb_time_id = 0;
    }
    ear_sports_handle_hr_data_send();
    tws_salve_push_all_day_data_finish_flag = 0;
    // 删除定时器

    // 清空链表
    if (list_empty(&salve_data_list_head) || salve_last_timestamp == 0) {
        return;
    }

    struct bulk_list *p = NULL;
    struct bulk_list *n = NULL;
    list_for_each_entry_safe(p, n, &salve_data_list_head, head) {
        list_del(&p->head);
        free(p);
        p = NULL;
    }
}

// 主机请求从机推数据
void master_get_all_day_data()
{
    if (tws_api_get_role()) {
        // printf("master_get_all_day_data, role is slave");
        return ;
    }
    // 是否有TWS连接
    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)) {
        // printf("TWS disconnected, can't push data");
        ear_sports_handle_hr_data_send();
        return ;      // TWS断连/未连接
    }
    // 主机问从机拿数据，从机推送数据
    u8 data = 0x01;
    tws_api_send_data_to_slave(&data, sizeof(data), TWS_FUNC_ID_MASTER_ALLDAY_DATA_REQ);
    // 定时检查tws从机是否传完数据
    if (!check_slave_data_push_finished_cb_time_id) {
        check_slave_data_push_finished_cb_time_id = sys_timer_add(NULL, master_check_slave_data_push_finished, 1000);
    }
}

// 推送全天检测的时间戳给从机
void rtc_imitate_add_5minutes()
{
    line_inf
    bool tws_timestamp_notify = 0;
    add_5minutes(&long_check_timestamp);
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role()) {
        return ;
    }
    if (!tws_timestamp_notify) {
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {    // TWS连接要推时间戳给对耳
            tws_sync_health_all_day_timestamp(&long_check_timestamp);
            tws_timestamp_notify = 1;
        }
    }
    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)) {
        // TWS断连或者未连接要清掉标志,表示下一次TWS连接要同步时间戳
        tws_timestamp_notify = 0;
    }
#endif
}

static void set_tws_sibling_health_all_day_timestamp(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    if (rx) {
        line_inf
        long_check_timestamp.year = data[0] << 8 || data[1];
        long_check_timestamp.month = data[2];
        long_check_timestamp.day = data[3];
        long_check_timestamp.hour = data[4];
        long_check_timestamp.minute = data[5];
        long_check_timestamp.second = data[6];
        // health_all_day_detect_switch(1);
        put_buf(data, 7);
        if (!rtc_imitate_cb_timer_id) {
            rtc_imitate_cb_timer_id = sys_timer_add(NULL, rtc_imitate_add_5minutes, HEALTH_ALL_DAY_DETECT_TIMEOUT * 60000);
        }
    }
}

REGISTER_TWS_FUNC_STUB(all_day_sync_stub) = {
    .func_id = TWS_FUNC_ID_ALLDAY_TIMESTAMP,
    .func    = set_tws_sibling_health_all_day_timestamp,
};

// 推送全天检测的时间戳给从机
void tws_sync_health_all_day_timestamp(TimeFields *numbers)
{
    u8 data[7];
    data[0] = numbers->year >> 8;
    data[1] = numbers->year & 0XFF;
    data[2] = numbers->month;
    data[3] = numbers->day;
    data[4] = numbers->hour;
    data[5] = numbers->minute;
    data[6] = numbers->second;
    if (numbers->year && numbers->month && numbers->day) {
        tws_api_send_data_to_sibling(data, 7, TWS_FUNC_ID_ALLDAY_TIMESTAMP);
        printf("tws_sync_health_all_day_timestamp\n");
    } else {
        printf("have not timestamp\n");
    }

    put_buf(data, 7);
}

#endif

#endif  // HEALTH_ALL_DAY_CHECK_ENABLE

