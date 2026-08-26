#ifndef RDX_HOST_TEST_SYSTEM_INCLUDES_H
#define RDX_HOST_TEST_SYSTEM_INCLUDES_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

u16 sys_timeout_add(void *priv, void (*callback)(void *), u32 timeout_ms);
void sys_timeout_del(u16 timer_id);

#endif
