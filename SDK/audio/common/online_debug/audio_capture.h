#ifndef _AUDIO_CAPTURE_H_
#define _AUDIO_CAPTURE_H_

#include "generic/typedef.h"

int audio_capture_init(void);
int audio_capture_exit(void);
int audio_capture_cbuf_write(u8 index, void *data, u32 len);

#endif/*_AUDIO_CAPTURE_H_*/
