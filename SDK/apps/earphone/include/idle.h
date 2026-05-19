#ifndef APP_MODE_IDLE_H
#define APP_MODE_IDLE_H

#include "app_msg.h"

enum {
    IDLE_MODE_CHARGE,
    IDLE_MODE_PLAY_POWEROFF,
    IDLE_MODE_WAIT_POWEROFF,
    IDLE_MODE_POWEROFF,
    IDLE_MODE_WAIT_DEVONLINE,
};

struct app_mode *app_enter_idle_mode(int arg);


#endif
