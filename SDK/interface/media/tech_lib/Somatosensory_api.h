#ifndef _SOMATOSENSORY_API_H_
#define _SOMATOSENSORY_API_H_

#define text_code __attribute__((section(".Somatosensory.text")))
#define cache_code __attribute__((section(".Somatosensory.text.cache.L2.code")))
#define const_code __attribute__((section(".Somatosensory.text.cache.L2.const")))

struct six_axis_para {
    float acc_off_x;
    float acc_off_y;
    float acc_off_z;
    float gro_off_x;
    float gro_off_y;
    float gro_off_z;
};

struct somato_control {
    int fs;
    float val_dif;          //运动检测灵敏度
    char main_axis;         //主轴（0-x 1-y 2-z）
    char plane_axis;        //水平轴（平行水平面的轴 0-x 1-y 2-z）
    short val_station;      //静态门限
    short val_nod;          //点头门限
    short val_turn;         //转头门限
    char sign_gyro1;        //重力轴方向竖直向上为+1，向下为-1
    char sign_gyro2;
    char sign_gyro3;
    short G_val;
};

int get_somatosensory_buf();
void init_somatosensory(void *ptr, struct six_axis_para *sap, struct somato_control *sc);
void reset_somatosensory(void *ptr);
int run_somatosensory(void *ptr, short *data, int len);

#endif

