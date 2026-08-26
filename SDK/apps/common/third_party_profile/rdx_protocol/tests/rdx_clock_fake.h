#ifndef RDX_CLOCK_FAKE_H
#define RDX_CLOCK_FAKE_H

#include <stdint.h>

#define RDX_CLOCK_FAKE_READ_PLAN_MAX 8

struct rdx_clock_fake_state {
    unsigned int read_count;
    unsigned int write_count;
    uint32_t last_write_timestamp;
};

void rdx_clock_fake_reset(void);
void rdx_clock_fake_push_read(int result, uint32_t timestamp,
                              uint8_t is_placeholder);
void rdx_clock_fake_set_write_result(int result);
const struct rdx_clock_fake_state *rdx_clock_fake_get_state(void);

#endif /* RDX_CLOCK_FAKE_H */
