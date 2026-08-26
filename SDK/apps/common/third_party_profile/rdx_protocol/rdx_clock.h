#ifndef RDX_CLOCK_H
#define RDX_CLOCK_H

#include <stdint.h>

struct rdx_clock_snapshot {
    uint32_t timestamp;
    uint8_t is_placeholder;
};

/*
 * Read and write the target's persistent wall clock. SDK calendar types,
 * conversion helpers and device handles stay private to the selected
 * adapter. A successful write only means the hardware accepted the value;
 * the RTC service owns the read-back policy.
 */
int rdx_clock_read(struct rdx_clock_snapshot *out);
int rdx_clock_write(uint32_t timestamp);

#endif /* RDX_CLOCK_H */
