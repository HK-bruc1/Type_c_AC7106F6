#ifndef _FMY_GSENSOR_H
#define _FMY_GSENSOR_H

#define  GSENSOR_PRINTF_ENABLE   1

// fmy gsensor api
bool fmy_motion_detection(void);
int  fmy_motion_detection_init(void);
int  fmy_motion_detection_deinit(void);

// Motion Detection algorithm in MotionDetection.a
int get_DetectionBuf(int fs);
void init_MotionDet(void *ptr, short fs, float thread);
char run_MotionDetection(void *ptr, int len, short *data);
#endif
