#ifndef __Q32DSP_CACHE__
#define __Q32DSP_CACHE__

#include "icache.h"
#include "rocache.h"

//#include "generic/typedef.h"

typedef struct __cache_info {
    unsigned int cache_type;		// 0:icache; 1:dcache
    unsigned int cpu_id;
    unsigned int efficiency;
} CACHE_INFO;

#if 0  // 供外界使用的接口,已经在 icache.h 和 rocache.h 中定义
// void flush_dcache(void *ptr, int len);
// void flushinv_dcache(void *ptr, int len);
void IcuEnable(void);
void DcuEnable(void);
void IcuWaitIdle(void);
void DcuWaitIdle(void);
void IcuDisable(void);
void DcuDisable(void);
void IcuFlushinvAll(void);
void IcuUnlockAll(void);
void IcuFlushinvRegion(int *beg, int len);
void IcuUnlockRegion(int *beg, int len);
void IcuLockRegion(int *beg, int len);
void IcuPfetchRegion(int *beg, int len);
void DcuFlushinvAll(void);
void DcuFlushinvRegion(int *beg, int len);
void DcuPfetchRegion(int *beg, int len);
void IcuInitial(void);
void DcuInitial(void);
#endif

#define WAIT_DCACHE_IDLE    do {DcuWaitIdle();} while(0);
#define WAIT_ICACHE_IDLE    do {IcuWaitIdle();} while(0);
//#define WAIT_DCACHE_IDLE    do{asm volatile("csync"); while(!(JL_DCU->CON & BIT(31)));} while(0);

#endif
