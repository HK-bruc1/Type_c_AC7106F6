#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".music_app_msg_handler.data.bss")
#pragma data_seg(".music_app_msg_handler.data")
#pragma const_seg(".music_app_msg_handler.text.const")
#pragma code_seg(".music_app_msg_handler.text")
#endif
#include "key_driver.h"
#include "app_main.h"
#include "app_default_msg_handler.h"
#include "init.h"
#include "dev_manager.h"
#include "music/music_player.h"
#include "music/breakpoint.h"
#include "app_music.h"
#include "scene_switch.h"
#include "node_param_update.h"
#include "clock_manager/clock_manager.h"
#include "scene_switch.h"
#include "mic_effect.h"
#include "local_tws.h"
#include "rcsp_device_status.h"
#include "rcsp_music_func.h"
#include "rcsp_config.h"

extern struct __music music_hdl;
//*----------------------------------------------------------------------------*/
/**@brief    music 模式解码错误处理
   @param    err:错误码，详细错误码描述请看MUSIC_PLAYER错误码表枚举
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void music_player_err_deal(int err)
{
    u16 msg = APP_MSG_NULL;
    char *logo = NULL;
    if (err != MUSIC_PLAYER_ERR_NULL && err != MUSIC_PLAYER_ERR_DECODE_FAIL) {
        music_hdl.file_err_counter = 0;///清除错误文件累计
    }

    if (err != MUSIC_PLAYER_ERR_NULL && err != MUSIC_PLAYER_SUCC) {
        log_e("music player err = %d\n", err);
    }

    switch (err) {
    case MUSIC_PLAYER_SUCC:
        music_hdl.file_err_counter = 0;
        break;
    case MUSIC_PLAYER_ERR_NULL:
        break;
    case MUSIC_PLAYER_ERR_POINT:
    /* fall-through */
    case MUSIC_PLAYER_ERR_NO_RAM:
        msg = APP_MSG_GOTO_NEXT_MODE;//退出音乐模式
        break;
    case MUSIC_PLAYER_ERR_DECODE_FAIL:
        if (music_hdl.file_err_counter >= music_hdl.player_hd->fsn->file_number) {
            music_hdl.file_err_counter = 0;
            dev_manager_set_valid_by_logo(dev_manager_get_logo(music_hdl.player_hd->dev), 0);///将设备设置为无效设备
            if (dev_manager_get_total(1) == 0) {//参数为1 ：获取所有有效设备  参数0：获取所有设备
                msg = APP_MSG_GOTO_NEXT_MODE;//没有设备了，退出音乐模式
            } else {
                msg = APP_MSG_MUSIC_AUTO_NEXT_DEV;///所有文件都是错误的， 切换到下一个设备
            }
        } else {
            music_hdl.file_err_counter ++;
            if (music_hdl.file_play_direct == 0) {
                msg = APP_MSG_MUSIC_NEXT;//播放下一曲
            } else {
                msg = APP_MSG_MUSIC_PREV;//播放上一曲
            }
        }
        break;
    case MUSIC_PLAYER_ERR_DEV_NOFOUND:
        log_e("MUSIC_PLAYER_ERR_DEV_NOFOUND \n");
        if (dev_manager_get_total(1) == 0) {//参数为1 ：获取所有有效设备  参数0：获取所有设备
            msg = APP_MSG_GOTO_NEXT_MODE;///没有设备在线， 退出音乐模式
        } else {
            msg = APP_MSG_MUSIC_PLAY_FIRST;///没有找到指定设备， 播放之前的活动设备
        }
        break;

    case MUSIC_PLAYER_ERR_FSCAN:
        ///需要结合music_player_scandisk_break中处理的标志位处理
        if (music_hdl.scandisk_break) {
            music_hdl.scandisk_break = 0;
            ///此处不做任何处理， 打断的事件已经重发， 由重发事件执行后续处理
            break;
        }
    case MUSIC_PLAYER_ERR_DEV_READ:
        log_e("MUSIC_PLAYER_ERR_DEV_READ \n");
    /* fall-through */
    case MUSIC_PLAYER_ERR_DEV_OFFLINE:
        log_e("MUSIC_PLAYER_ERR_DEV_OFFLINE \n");
        logo = dev_manager_get_logo(music_hdl.player_hd->dev);
        if (dev_manager_online_check_by_logo(logo, 1)) {
            ///如果错误失败在线， 并且是播放过程中产生的，先记录下断点
            if (music_player_get_playing_breakpoint(music_hdl.player_hd, music_hdl.breakpoint, 1) == true) {
                music_player_stop(music_hdl.player_hd, 0);//先停止，防止下一步操作VM卡顿
                breakpoint_vm_write(music_hdl.breakpoint, logo);
            }
            if (err == MUSIC_PLAYER_ERR_FSCAN) {
                dev_manager_set_valid_by_logo(logo, 0);///将设备设置为无效设备
            } else {
                //针对读错误， 因为时间推到应用层有延时导致下一个模式判断不正常， 此处需要将设备卸载
                dev_manager_unmount(logo);
            }
        }
        if (dev_manager_get_total(1) == 0) {
            /* app_status_handler(APP_STATUS_MUSIC_QUIT); */
            msg = APP_MSG_GOTO_NEXT_MODE;///没有设备在线， 退出音乐模式
        } else {
            msg = APP_MSG_MUSIC_AUTO_NEXT_DEV;///切换设备
        }
        break;
    case MUSIC_PLAYER_ERR_FILE_NOFOUND:
        ///查找文件有扫盘的可能，也需要结合music_player_scandisk_break中处理的标志位处理
        if (music_hdl.scandisk_break) {
            music_hdl.scandisk_break = 0;
            ///此处不做任何处理， 打断的事件已经重发， 由重发事件执行后续处理
            break;
        }
    case MUSIC_PLAYER_ERR_PARM:
        logo = dev_manager_get_logo(music_hdl.player_hd->dev);
        if (dev_manager_online_check_by_logo(logo, 1)) {
            if (music_hdl.player_hd->fsn->file_number) {
                msg = APP_MSG_MUSIC_PLAY_FIRST;///有文件,播放第一个文件
                break;
            }
        }

        if (dev_manager_get_total(1) == 0) {
            msg = APP_MSG_GOTO_NEXT_MODE;//没有设备了，退出音乐模式
        } else {
            msg = APP_MSG_MUSIC_AUTO_NEXT_DEV;
        }
        break;
    case MUSIC_PLAYER_ERR_FILE_READ://文件读错误
        msg = APP_MSG_MUSIC_NEXT;//播放下一曲
        break;
    }
    if (msg != APP_MSG_NULL) {
        app_send_message(msg, 0);
    }
}


int music_app_msg_handler(int *msg)
{
    int err = MUSIC_PLAYER_ERR_NULL;
    char *logo = NULL;
    if (false == app_in_mode(APP_MODE_MUSIC)) {
        return 0;
    }
    u8 auto_next_dev;
    struct file_player *file_player = get_music_file_player();
    switch (msg[0]) {
    case APP_MSG_CHANGE_MODE:
        printf("app msg key change mode\n");
        app_send_message(APP_MSG_GOTO_NEXT_MODE, 0);
        break;
    case APP_MSG_MUSIC_PP:
        printf("app msg music pp\n");
        music_file_player_pp(file_player);
        app_send_message(APP_MSG_MUSIC_PLAY_STATUS, music_file_get_player_status(file_player));
        break;
    case APP_MSG_MUSIC_SPEED_UP:
        printf("app msg music speed up\n");
        music_hdl.speed_mode = music_file_speed_up(file_player);
        clock_refurbish();
        break;
    case APP_MSG_MUSIC_SPEED_DOWN:
        printf("app msg music speed down\n");
        music_hdl.speed_mode = music_file_speed_down(file_player);
        clock_refurbish();
        break;
    case APP_MSG_PITCH_UP:
        printf("app msg music pitch up\n");
        music_hdl.pitch_mode = music_file_pitch_up(file_player);
        break;
    case APP_MSG_PITCH_DOWN:
        printf("app msg music pitch down\n");
        music_hdl.pitch_mode = music_file_pitch_down(file_player);
        break;
    case APP_MSG_MUSIC_CHANGE_REPEAT:
        music_player_change_repeat_mode(music_hdl.player_hd);
        app_send_message(APP_MSG_REPEAT_MODE_CHANGED, music_player_get_repeat_mode());
        break;
    case APP_MSG_MUSIC_PLAYER_AB_REPEAT_SWITCH:
        printf("app msg music ab repeat switch\n");
#if FILE_DEC_AB_REPEAT_EN
        music_file_ab_repeat_switch(file_player);
#endif
        break;
    case APP_MSG_MUSIC_PLAY_FIRST:
        printf("music_player_play_first_file\n");
        err = music_player_play_first_file(music_hdl.player_hd, NULL);
        break;
    case APP_MSG_MUSIC_NEXT:
        printf("app msg music next\n");

        mem_stats();
        /* app_status_handler(APP_STATUS_MUSIC_FFR); */
        music_hdl.file_play_direct = 0;
        err = music_player_play_next(music_hdl.player_hd);
        break;
    case APP_MSG_MUSIC_PREV:
        printf("app msg music prev\n");
        mem_stats();
        /* app_status_handler(APP_STATUS_MUSIC_FFR); */
        music_hdl.file_play_direct = 1;
        err = music_player_play_prev(music_hdl.player_hd);
        break;

    case APP_MSG_MUSIC_PLAYE_PREV_FOLDER:
        err = music_player_play_folder_prev(music_hdl.player_hd);
        break;
    case APP_MSG_MUSIC_PLAYE_NEXT_FOLDER:
        err = music_player_play_folder_next(music_hdl.player_hd);
        break;
    case APP_MSG_MUSIC_AUTO_NEXT_DEV:
    /* fall-through */
    case APP_MSG_MUSIC_CHANGE_DEV:
        log_i("KEY_MUSIC_CHANGE_DEV\n");
        auto_next_dev = ((msg[0] == APP_MSG_MUSIC_AUTO_NEXT_DEV) ? 1 : 0);
        logo = music_player_get_dev_next(music_hdl.player_hd, auto_next_dev);
        if (logo == NULL) { ///找不到下一个设备，不响应设备切换
            break;
        }
        printf("next dev = %s\n", logo);
        ///切换设备前先保存一下上一个设备的断点信息,包括文件和解码信息
        if (music_player_get_playing_breakpoint(music_hdl.player_hd, music_hdl.breakpoint, 1) == true) {
            music_player_stop(music_hdl.player_hd, 0); //先停止，防止下一步操作VM卡顿
            breakpoint_vm_write(music_hdl.breakpoint, dev_manager_get_logo(music_hdl.player_hd->dev));
        }
#if (TCFG_MUSIC_DEVICE_TONE_EN)
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
        const char *file_name = get_music_tone_name_by_logo(logo);
        if (file_name != NULL) {
            if (local_tws_enter_mode(file_name, (void *)logo) == 0) {
                break;
            } else {
                if (music_device_tone_play(logo) == true) {
                    break;
                }
            }
        }
#else
        if (music_device_tone_play(logo) == true) {
            break;
        }
#endif
#endif
        if (true == breakpoint_vm_read(music_hdl.breakpoint, logo)) {
            err = music_player_play_by_breakpoint(music_hdl.player_hd, logo, music_hdl.breakpoint);
        } else {
            err = music_player_play_first_file(music_hdl.player_hd, logo);
        }
        break;

#if TCFG_MIX_RECORD_ENABLE
    //只有使能录音宏，才会出现删文件的操作，此时可能会出现删了最后一个文件的情况
    case  APP_MSG_MUSIC_DELETE_CUR_FILE:
        // 删除当前文件
        int ret = music_player_delete_playing_file(music_hdl.player_hd);
        if (ret != MUSIC_PLAYER_SUCC) {
            //播放失败，此时可能删除了最后一个可播放的文件，处理是：切换模式
            r_printf("[Music Err]: case APP_MSG_MUSIC_DELETE_CUR_FILE, ret: %d, will switch mode!\n", ret);
            app_send_message(APP_MSG_CHANGE_MODE, 0);
        }
        break;
#endif

#if TCFG_APP_RECORD_EN
    case  APP_MSG_MUSIC_PLAY_REC_FOLDER_SWITCH:
        log_i("KEY_MUSIC_PLAYE_REC_FOLDER_SWITCH\n");
        // 切换到录音文件夹里播录音
        char *_logo = dev_manager_get_logo(music_hdl.player_hd->dev);
        char folder_path[32] = {};
        sprintf(folder_path, "%s%s%s", "/", TCFG_REC_FOLDER_NAME, "/");
        if (_logo == NULL) {
            r_printf("Err: logo==NULL, %d\n", __LINE__);
            err = MUSIC_PLAYER_ERR_RECORD_DEV;
        } else {
            printf(">>> logo: %s, folder_path: %s\n", _logo, folder_path);		//udisk0
            err = music_player_play_by_path(music_hdl.player_hd, _logo, "/JL_REC/");
        }
#endif

        break;
    case APP_MSG_MUSIC_PLAY_START_BY_DEV:
#if (TCFG_MUSIC_DEVICE_TONE_EN)
        logo = (char *)msg[1];
        log_i("KEY_MUSIC_DEVICE_TONE_END %s\n", logo);
        if (logo) {
            if (true == breakpoint_vm_read(music_hdl.breakpoint, logo)) {
                err = music_player_play_by_breakpoint(music_hdl.player_hd, logo, music_hdl.breakpoint);
            } else {
                err = music_player_play_first_file(music_hdl.player_hd, logo);
            }
        }
        break;
#endif
    case APP_MSG_MUSIC_MOUNT_PLAY_START:
        logo = (char *)msg[1];
        log_i("APP_MSG_MUSIC_MOUNT_PLAY_START %s\n", logo);
        dev_manager_set_active_by_logo(logo);
    /* fall-through */
    case APP_MSG_MUSIC_PLAY_START:
        log_i("APP_MSG_MUSIC_PLAY_START !!\n");
#if 1
        /* app_status_handler(APP_STATUS_MUSIC_PLAY); */
        ///断点播放活动设备
        logo = dev_manager_get_logo(dev_manager_find_active(1));
        if (music_player_runing()) {
            if (dev_manager_get_logo(music_hdl.player_hd->dev) && logo) {///播放的设备跟当前活动的设备是同一个设备，不处理
                if (0 == strcmp(logo, dev_manager_get_logo(music_hdl.player_hd->dev))) {
                    log_w("the same dev!!\n");
                    break;
                }
            }
        }
#if (TCFG_MUSIC_DEVICE_TONE_EN && TCFG_LOCAL_TWS_ENABLE == 0)
        if (music_device_tone_play(logo) == true) {
            break;
        }
#endif
        log_i("APP_MSG_MUSIC_PLAY_START %s\n", logo);
        if (true == breakpoint_vm_read(music_hdl.breakpoint, logo)) {
            err = music_player_play_by_breakpoint(music_hdl.player_hd, logo, music_hdl.breakpoint);
        } else {
            err = music_player_play_first_file(music_hdl.player_hd, logo);
        }
#endif
        break;
    case APP_MSG_MUSIC_PLAY_START_BY_SCLUST:
        log_i("KEY_MUSIC_PLAYE_BY_DEV_SCLUST\n");
        logo = dev_manager_get_logo(dev_manager_find_active(1));
        err = music_player_play_by_sclust(music_hdl.player_hd, logo, msg[1]);
        break;
    case APP_MSG_MUSIC_FR:
        printf("app msg music fr \n");
        music_file_player_fr(3, file_player);
        break;
    case APP_MSG_MUSIC_FF:
        printf("app msg music ff\n");
        music_file_player_ff(3, file_player);
        break;
    case APP_MSG_MUSIC_PLAYER_END:
        err = music_player_end_deal(music_hdl.player_hd, msg[1]);
        break;
    case APP_MSG_MUSIC_DEC_ERR:
        err = music_player_decode_err_deal(music_hdl.player_hd, msg[1]);
        break;
    case APP_MSG_MUSIC_PLAY_BY_NUM:
        printf("APP_MSG_MUSIC_PLAY_BY_NUM:%d\n", msg[1]);
        logo = dev_manager_get_logo(dev_manager_find_active(1));
        err = music_player_play_by_number(music_hdl.player_hd, logo, msg[1]);
        break;
    case APP_MSG_MUSIC_PLAY_START_AT_DEST_TIME:
#if FILE_DEC_DEST_PLAY
        if (music_player_runing()) {
            //for test 测试MP3定点播放功能
            puts("\n play start at 5s \n");
            file_dec_set_start_play(5000, AUDIO_CODING_MP3);
        }
#endif
        break;
#if 0
    case APP_MSG_VOCAL_REMOVE:
        printf("APP_MSG_VOCAL_REMOVE\n");
        music_vocal_remover_switch();
        break;
    case APP_MSG_MIC_EFFECT_ON_OFF:
        printf("APP_MSG_MIC_EFFECT_ON_OFF\n");
        if (mic_effect_player_runing()) {
            mic_effect_player_close();
        } else {
            mic_effect_player_open();
        }
        break;
#endif
    default:
        app_common_key_msg_handler(msg);
        break;
    }
    music_player_err_deal(err);



#if (TCFG_APP_MUSIC_EN && !RCSP_APP_MUSIC_EN)
    //TODO: MUSIC_INFO_ATTR_STATUS宏找不到
    //rcsp_device_status_update(MUSIC_FUNCTION_MASK,
    //                          BIT(MUSIC_INFO_ATTR_STATUS) | BIT(MUSIC_INFO_ATTR_FILE_PLAY_MODE));
#endif


    return 0;
}

