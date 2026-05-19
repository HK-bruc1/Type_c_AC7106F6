#ifndef APP_DEFAULT_MSG_HANDLER_H
#define APP_DEFAULT_MSG_HANDLER_H


#include "system/includes.h"

int app_default_msg_handler(int *msg);
void app_common_key_msg_handler(int *msg);
int app_common_device_event_handler(int *msg);

#endif

