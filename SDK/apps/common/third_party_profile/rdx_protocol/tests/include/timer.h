#ifndef RDX_HOST_TEST_TIMER_H
#define RDX_HOST_TEST_TIMER_H

#include <stdint.h>

uint16_t sys_timeout_add(void *priv, void (*callback)(void *),
                         uint32_t timeout_ms);
void sys_timeout_del(uint16_t timer_id);

#endif /* RDX_HOST_TEST_TIMER_H */
