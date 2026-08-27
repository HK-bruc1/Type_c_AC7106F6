#ifndef RDX_HOST_TEST_SDFILE_H
#define RDX_HOST_TEST_SDFILE_H

#include <stdint.h>

uint32_t sdfile_get_disk_capacity(void);
void *sdfile_flash_addr2cpu_addr(uint32_t addr);

#endif
