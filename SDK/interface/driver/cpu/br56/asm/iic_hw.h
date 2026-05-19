#ifndef __IIC_HW_H__
#define __IIC_HW_H__

#define MAX_HW_IIC_NUM                  1
#define P11_HW_IIC_NUM                  0 //p11 iic使能,及锁使能
// typedef enum {
//     HW_IIC_0,
//     // HW_IIC_1,
// } hw_iic_dev;

enum {
    HW_IIC_0,
    // HW_IIC_1,
#if defined(P11_HW_IIC_NUM)&&P11_HW_IIC_NUM
    HW_P11_IIC_0,//p11 init,pb
#endif
};


#endif

