#pragma once

#define algo_log(fmt,...)  printf("[algo] %s " fmt "\n",__func__, ##__VA_ARGS__)

#define  SLEEP_MAX_BLOCK          (100)


#pragma pack(1)

typedef union {
    struct {
        short step_counter: 1;   //计步器
        short distance: 1;       //距离
        short step_frequency: 1; //步频
        short calories: 1;       //卡路里 = 基础代谢率（BMR） + 活动代谢率（AMR）
        short calories_amr: 1;   //活动代谢率（AMR）：活动代谢率是指在运动和日常活动中所消耗的能量
        short sedentary: 1;      //久坐提醒
        short activity_type: 1;  //活动类型
        short sleep: 1;          //睡眠
        short shake: 1;          //摇一摇
    };
    short all;
} algo_type;

typedef struct {
    short height;          //身高 cm
    short weight;          //体重 kg
    short gender;          //性别 0 man 1 female
    short ages;            //年龄
    short step_factor;     //步长系数 一般40~60
} user_info;


typedef struct {
    short x;               //gsensor x轴数据
    short y;               //gsensor y轴数据
    short z;               //gsensor z轴数据
} accel_data;

typedef struct {
    short lsb_g;           //gsensor参数 1g对应的数值
    short sps;             //gsensor参数 每秒采样次数
} accel_config;

typedef enum {
    ACTIVITY_UNKNOWN,      //活动：未知
    ACTIVITY_STILL,        //活动：静止
    ACTIVITY_WALK,         //活动：走路
    ACTIVITY_RUN,          //活动：跑步
    ACTIVITY_CYCLING,      //活动：骑行
    ACTIVITY_BASKETBALLL,  //活动：篮球
    ACTIVITY_BADMINTON,    //活动：羽毛球
} activity_type;

typedef enum {
    SLEEP_STAGE_AWAKE,
    SLEEP_STAGE_LIGHT,
    SLEEP_STAGE_DEEP,
    SLEEP_STAGE_REM,
} sleep_type;

typedef struct {
    unsigned int  timestamp;
    unsigned char stage;
} sleep_chart;

typedef struct {
    sleep_chart   chart[SLEEP_MAX_BLOCK];
    unsigned char blocks;
} sleep_data;

typedef struct {
    algo_type update;          //算法更新标志
    int       steps;           //步数
    int       sedentary;       //久坐时间，单位 秒
    int       calories;        //卡路里 = 基础代谢率（BMR） + 活动代谢率（AMR） 单位 卡
    int       calories_amr;    //活动代谢率（AMR） 单位 卡
    int       activity;        //活动类型，参考 activity_type
    int       distance;        //距离, 单位cm
    int       step_frequency;  //步频，单位spm (步/分钟)
    int       sleep;           //睡眠, 参考sleep_type
    int       shake;           //摇晃次数
} algo_out;

#pragma pack()
/* --------------------------------------------------------------------------*/
/**
 * @brief 运动算法初始化
 *
 * @param [in] open   选择需要开启的算法
 * @param [in] accel  gsensor配置
 * @param [in] user   用户信息
 *
 */
void algo_motion_init(algo_type open, accel_config accel, user_info user);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法运行函数
 *
 * @param [in] *accel       gsensor 3轴数据
 * @param [in] accel_point  gsensor 数据点数
 * @param [in] *rri         R-R间隔
 * @param [in] rri_point    R-R间隔点数
 * @param [out]             算法输出，见 algo_out 定义
 *
 */
algo_out algo_motion_run(unsigned int utc, accel_data *accel, short accel_point,  short *rri, short rri_point);


/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法设置佩戴状态
 *
 * @param [in]    wear  0:未佩戴 1：佩戴
 * @note  充电时应设置为“未佩戴”
 *
 */
void algo_motion_wear_set(unsigned char wear);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法睡眠是否终止
 *
 * @param [out]   1：睡眠终止  0：睡眠进行中
 *
 */
unsigned char algo_motion_sleep_is_terminated(void);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法获取睡眠数据
 *
 * @param [out]             睡眠数据，见 sleep_data 定义
 * @note  调用本接口会强制睡眠终止，除非用户手动查看睡眠数据，否则应先查询是否睡眠终止，
 * 睡眠终止后再获取数据
 */
void algo_motion_sleep_get(sleep_data *out);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法设置睡眠参数
 * @param [in] unsigned char thres[4]   输入4个参数
 * @note  正常不需要调用该接口
 */
void algo_motion_sleep_set(unsigned char thres[4]);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法设置放置桌面识别参数
 * @param [in] unsigned char thres[3]   输入3个参数
 * @note  正常不需要调用该接口
 */
void algo_motion_on_desk_set(unsigned char thres[3]);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法清除睡眠数据
 */
void algo_motion_sleep_clean(void);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法设置计步参数
 * @param [in] unsigned char thres 范围:0~100 建议:30~50
 * @note  正常不需要调用该接口
 */
void algo_moiton_step_counter_set(unsigned char thres);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法获取睡眠特征数据
 *
 * @param [out] unsigned char*   1440笔睡眠特征数据
 * @note  如果睡眠结果不准确，请保存睡眠特征数据及记录正确的入睡和出睡时间，提供给杰理
 */
unsigned char *algo_motion_sleep_get_feature(void);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法生成睡眠数据
 *
 * @note  生成模拟数据，用于调试
 */
void algo_motion_sleep_generate_data(void);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法开启/关闭摇一摇识别
 *
 * @param [in] enable       1：开启   0：关闭
 * @note  在特定页面开启，开启摇一摇会停止计步功能
 *
 */
void algo_motion_enable_shake(unsigned char enable);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动手势算法获取就绪状态
 *
 * @param [out]    1：就绪   0：未就绪
 * @note  手势算法就绪时，识别将开始，需要关闭马达振动和响铃
 *
 */
short algo_motion_gesture_is_ready(void);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法开启/关闭轻触手势功能
 *
 * @param [in] enable       1：开启   0：关闭
 * @note  在特定页面开启，如来电界面用来接听电话
 *
 */
void algo_motion_enable_tap_gesture(unsigned char enable);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法获取轻触手势功能
 *
 * @param [out]    1：触发   0：未触发
 * @note  在特定页面开启，如来电界面用来接听电话
 *
 */
short algo_motion_get_tap_gesture(void);

/* -------------------------------------------------------------------------*/
/**
 * @brief 运动算法 获取版本号
 *
 * @param [out] 版本号
 * @note  10 代表v1.0
 * @note  101 代表v10.1
 *
 */
int algo_motion_get_version(void);
