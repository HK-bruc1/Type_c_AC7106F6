#ifndef _LINEIN_H_
#define _LINEIN_H_


/**
 * @brief linein音量增
 */
void linein_key_vol_up();

/**
 * @brief linein音量减
 */
void linein_key_vol_down();

/**
 * @brief 获取linein 播放状态
 * @return 1:当前正在打开 0：当前正在关闭
 */
u8 linein_get_status(void);

/**
 * @brief    linein 播放暂停切换
 * @return   播放暂停状态
 */
int linein_pp(void);

/**
 * @brief linein 音量设置函数
 * @param 需要设置的音量
 * @note 在linein 情况下针对了某些情景进行了处理，设置音量需要使用独立接口
 */
void linein_volume_set(s16 vol);

/**
 * @brief   进入linein模式
 * @param   arg 附加参数
 * @return  下一个要进入的模式
 */
struct app_mode *app_enter_linein_mode(int arg);

#endif


