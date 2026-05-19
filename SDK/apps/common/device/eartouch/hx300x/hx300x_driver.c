#include "app_config.h"
#include "clock.h"
#include "os/os_api.h"
#include "gpio.h"
#include "eartouch_manage.h"
#include "eartouch_iic_interface.h"

#if(TCFG_OUTSIDE_EARTCH_ENABLE == 1 && TCFG_EARTCH_SENSOR_SEL == EARTCH_HX300X)

#define LOG_TAG             "[HX300X]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

//收发数中间的阻塞式延时
#define HX300X_IIC_DELAY       50

//chip id
const static u8 chip_id = 0x88;

static eartouch_platform_data *platform_data = NULL;

uint32_t Table_ps_data1[10];
uint32_t Table_ps_data2[10];

u8 TYHX_Read_REG(u8 register_address)
{
    u8 data = 0;
    u8 data_len = 1;
    irSensor_iic_read(chip_id, register_address, &data, data_len);
    r_printf("ir read data:%d\n", data);

    return data;
}


u8 TYHX_Write_REG(u8 register_address, u8 function_command)
{
    return irSensor_iic_write(chip_id, register_address, function_command);
}

void Sort(uint32_t *a, int len)
{
    int i = 0;
    int j;
    int t;
    for (i = 0; i < len - 1; i++) {
        for (j = 0; j < len - i - 1; j++) {
            if (a[j] > a[j + 1]) {
                t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
            }
        }
    }
}

uint32_t read_ps_data(void)
{
    uint8_t i = 0, j = 0;
    uint32_t ps_data1 = 0, ps_data1_temp = 0;
    uint8_t  databuf[2];


    for (i = 0; i < 5; i++) {

        databuf[0] = TYHX_Read_REG(0x07);
        databuf[1] = TYHX_Read_REG(0x08);
        ps_data1_temp = ((databuf[0]) | (databuf[1] << 8));
        //printf("/////ps1=%d\r\n",ps_data1_temp);
        Table_ps_data1[i] = ps_data1_temp;
        delay_nops(10);
    }

    Sort(Table_ps_data1, 5);
    for (j = 1; j < 4; j++) {
        ps_data1 = ps_data1 + Table_ps_data1[j];
    }
    ps_data1 = ps_data1 / 3;
    //printf("////////////////////\r\n");
    return ps_data1;
}

//  数据消除抖动，一次耗时约 4*(55ms + delay_time_ms) ms
uint32_t get_ps_data(uint8_t delay_time_ms)
{
    uint8_t i = 0, j = 0;
    uint32_t ps_data1 = 0, ps_data1_temp = 0;
    uint8_t  databuf[2];


    for (i = 0; i < 10; i++) {

        databuf[0] = TYHX_Read_REG(0x07);
        databuf[1] = TYHX_Read_REG(0x08);
        ps_data1_temp = ((databuf[0]) | (databuf[1] << 8));
        //SEGGER_RTT_printf(0,"/////ps1=%d\r\n",ps_data1_temp);
        Table_ps_data1[i] = ps_data1_temp;
        delay_nops(delay_time_ms);
    }

    Sort(Table_ps_data1, 10);
    for (j = 2; j < 7; j++) {
        ps_data1 = ps_data1 + Table_ps_data1[j];
    }
    ps_data1 = ps_data1 / 5;
    //SEGGER_RTT_printf(0,"////////////////////\r\n");
    r_printf("ps_data1:%d", ps_data1);
    return ps_data1;
}
uint16_t leddr = 0x20, H_THOD = 2133, L_THOD = 1878;
uint8_t reg_13_new = 0x15, gain_r0x12 = 0x60;
uint16_t k = 0;
uint16_t H_PS = 1600, L_PS = 1200;
uint8_t Reg10 = 0x20, Reg12 = 0x60, Reg13 = 0x15;
int32_t read_ps_1f, read_ps_10, read_ps_k, read_ps2 = 0, read_ps3 = 500;
//-------------------------------------------------
uint8_t HX300x_Calibration()
{
#if 0
    u8 ret = 0;

    printf("\r\n>>第一步：IIC & INT 检测！请按键....\r\n");
    while (START_IN); //START_IN：即所发的SPP命令
    ret = Calibration_First_IIC_INT_check();
    if (ret != 1) {
        return 0;
    }
    printf("\r\n>>第二步：对空测试！请按键....\r\n");
    while (START_IN);
    ret = Calibration_Second_OpenAir();
    if (ret != 1) {
        return 0;
    }
    printf("\r\n>>第三步：上灰卡测试！请按键....\r\n");
    while (START_IN);
    ret = Calibration_Third_GreyCard();
    if (ret == 0) {
        return 0;
    }
    printf("\r\n>>第四步：撤灰卡测试！请按键....\r\n");
    while (START_IN);
    ret = Calibration_Fourth_RemoveGreyCard();
    if (ret != 1) {
        return 0;
    }
    printf("\r\n>>第五步：保存，请按键....\r\n");
    while (START_IN);
    Calibration_Fifth_SaveDataToFlash();
#endif
    return 0;
}

//======================================================================
//======================================================================
int16_t Calibration_Second_OpenAir()
{
    //对空消底噪
    int16_t ii = 0;
    int32_t read_ps1 = 0;
    //int32_t read_ps_10 = 0;

    TYHX_Write_REG(0x00, 0x01);
    TYHX_Write_REG(0x01, 0x12);  //2a
    TYHX_Write_REG(0x02, 0x50);
    TYHX_Write_REG(0x03, 0xCD);
    TYHX_Write_REG(0x10, 0x1f);
    TYHX_Write_REG(0x11, 0xF7);
    TYHX_Write_REG(0x12, 0x70);   //90
    TYHX_Write_REG(0x13, 0x10);
    TYHX_Write_REG(0x20, 0x88);
    TYHX_Write_REG(0x21, 0xD3);
    TYHX_Write_REG(0x16, 0x20);
    TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
/////////////////////////////////////////////////////////////////////////////////////////////////
    //printf("\r\n...对空测试结构光开始..\r\n");
//cal K
    delay_nops(35);
    read_ps1 = get_ps_data(35);
    //printf("\r\nOFFSET DAC_R13 = %02x, STR DATA = %4d\r\n", (0x10|ii), read_ps1);
    ii = 0;
    while (((read_ps1 > 704) && (0 <= ii) && (ii <= 15))) {
        TYHX_Write_REG(0x00, 0x01);
        TYHX_Write_REG(0x13, (0x10 | ii));
        TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
        delay_nops(35);

        read_ps1 = get_ps_data(35);
        //printf("\r\nOFFSET DAC_R13 = %02x, STR DATA = %4d\r\n", (0x10|ii), read_ps1);

        ii++;
    }

    reg_13_new = TYHX_Read_REG(0x13);

    TYHX_Write_REG(0x00, 0x01);
    TYHX_Write_REG(0x10, 0x10);
    TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
    delay_nops(35);

    read_ps_10 = get_ps_data(35);

    TYHX_Write_REG(0x00, 0x01);
    TYHX_Write_REG(0x10, 0x1f);
    TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
    delay_nops(35);

    read_ps_1f = get_ps_data(35);

    k = (read_ps_1f - read_ps_10) / 16; //????

    if (k < 0) {
        // printf("\r\nThe Calculated Str Gain Ratio K Forced to Zero!!!\r\n");
        k = 0;
    }
    return 1;
}
//=====================================================================================
#if 0
int16_t Calibration_Third_GreyCard()
{
    // 灰卡测试
    uint8_t ii = 0;
    uint8_t k_sel = 0;
    uint16_t target_ps = (L_PS + H_PS) / 2;
    int32_t data_ps = 0;
    int16_t MAXleddr, MINleddr;
    uint8_t Reg[4];

    //printf("\r\n...灰卡测试开始　\r\n");

//cal gain
    data_ps = get_ps_data(35) - read_ps_1f;
    printf("target_ps = %4d, data_ps = %4d\r\n", target_ps, data_ps);

    if (data_ps != 0) {
        gain_r0x12 = ((10 * target_ps / data_ps) * 8 + 5) / 10;
    }

    if (gain_r0x12 >= 1) {
        gain_r0x12 = gain_r0x12 - 1;
    } else {
        gain_r0x12 = 0;
    }
    printf("Gain r0x12 = %4d\r\n", gain_r0x12);
    if (gain_r0x12 < 3) {
        gain_r0x12 = 3;
    }
    if (gain_r0x12 > 15) {
        gain_r0x12 = 15;
    }

    gain_r0x12 = gain_r0x12 << 4; //gain 写到寄存器高4位
    printf("Gain r0x12 shift = 0x%x\r\n", gain_r0x12);
    TYHX_Write_REG(0x12, gain_r0x12);

//find data ps in the defined range ////////////
    MAXleddr = 48;//
    MINleddr = 16;//
    TYHX_Write_REG(0x13, reg_13_new);

    for (ii = 0; ii <= 4; ii++) {
        leddr = ((MINleddr + MAXleddr) >> 1 & 0x3f);
        TYHX_Write_REG(0x00, 0x01);
        TYHX_Write_REG(0x10, leddr);
        TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
        delay_nops(35);

        read_ps2 = get_ps_data(35);

        read_ps_k = ((leddr - 16) * k + read_ps_10) * ((gain_r0x12 >> 4) + 1) / 8;

        data_ps = read_ps2 - read_ps_k;
        printf("read_ps2= %4d;leddr_Reg0x10 = 0x%02x; DATA_PS = %4d \
k2 = %d; 1f1x = %4d;2f1x = %d \r\n", \
               read_ps2, TYHX_Read_REG(0x10), data_ps, k, read_ps_10, read_ps_1f);
        if (data_ps > (H_PS + L_PS) / 2) {
            MAXleddr = leddr;//
        } else {
            MINleddr = leddr;
        }
    }

    Reg[0] = TYHX_Read_REG(0x10);
    Reg[1] = TYHX_Read_REG(0x11);
    Reg[2] = TYHX_Read_REG(0x12);
    Reg[3] = TYHX_Read_REG(0x13);
    printf("\r\n...R10 = 0x%x,R11 = 0x%x,R12 = 0x%x,R13 = 0x%x, \r\n", Reg[0],
           Reg[1], Reg[2], Reg[3]);
    printf("\r\n...data_ps Code = %4d,理论结构光 = %4d \r\n", data_ps, read_ps_k);
    printf("\r\n");

    //data_ps:灰卡增量值 ，read_ps_k：理论结构光感值
    return data_ps, read_ps_k; 		//上报到机台显示的数据
}
//=====================================================================================
int16_t Calibration_Fourth_RemoveGreyCard()
{
    //撤灰卡
    uint16_t str_diff = 0;
    uint8_t str_diff_t = 1;   //difference of struct light
    uint8_t h_thod_t = 1;     //high threshold test
    uint8_t l_thod_t = 1;     //low threshold test
    //int32_t read_ps3 = 0;


    TYHX_Write_REG(0x00, 0x01);
    TYHX_Write_REG(0x10, leddr);
    TYHX_Write_REG(0x11, 0xF7);
    TYHX_Write_REG(0x12, gain_r0x12); //90
    TYHX_Write_REG(0x13, reg_13_new);
    TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
    delay_nops(100);

    read_ps3 = get_ps_data(35);
    //

    if (read_ps3 > read_ps_k) {
        str_diff = read_ps3 - read_ps_k;
    } else {
        str_diff = read_ps_k - read_ps3;
    }
    printf("\r\n !!! str_diff = %4d\r\n", str_diff);

    //
    H_THOD =  read_ps2;
    L_THOD =  H_THOD - 300; //出耳阈值

    Reg10 = TYHX_Read_REG(0x10);
    //Reg11 =TYHX_Read_REG(0x11);
    Reg12 = TYHX_Read_REG(0x12);
    Reg13 = TYHX_Read_REG(0x13);


    printf("\r\n...R10 = 0x%x,R12 = 0x%x,R13 = 0x%x, \r\n", Reg10,
           Reg12, Reg13);

    return Reg10, Reg12, Reg13, H_THOD, read_ps3; //上报到机台显示的数据
}
//====================================================================
uint8_t Calibration_Fifth_SaveDataToFlash()
{
    //注：将以下参数保存到主控的flash里
    //Flash.CaliFlage = CaliFlage; //CaliFlage:光感已经校准过标志位
    //Flash.Reg10 = Reg10;	//Reg10 = leddr
    //Flash.Reg12 = Reg12;	//Reg12 = gain_r0x12
    //Flash.Reg13 = Reg13;	//Reg13 = reg_13_new
    //Flash.H_THOD = H_THOD;
    //Flash.L_THOD = L_THOD;
    //Flash.str_data = read_ps3;
    return 1;
}
//====================================================================
#endif

void hx300x_Set_Thres(int16_t h_thrd, int16_t l_thrd)
{
    TYHX_Write_REG(0x04, (uint8_t)(h_thrd >> 4));
    TYHX_Write_REG(0x05, 16 * (h_thrd >> 12) + (l_thrd >> 12));
    TYHX_Write_REG(0x06, (uint8_t)(l_thrd >> 4));
}
void hx300x_Set_cur(void)
{
    TYHX_Write_REG(0x10, leddr);       //Flash 保存的leddr
    TYHX_Write_REG(0x13, reg_13_new);  //Flash 保存的reg_13_new
}

void hx300x_Set_gain(void)
{
    TYHX_Write_REG(0x12, gain_r0x12);  //Flash 保存的gain
}
void HX300x_init(void)
{
    r_printf("----------------hx300x_init---------------");
    TYHX_Write_REG(0x00, 0x01);
    TYHX_Write_REG(0x01, 0x52);
    TYHX_Write_REG(0x02, 0x50);
    TYHX_Write_REG(0x03, 0x4d);

    hx300x_Set_Thres(H_THOD, L_THOD);
    hx300x_Set_cur();
    hx300x_Set_gain();

    TYHX_Write_REG(0x11, 0xF7);

    TYHX_Write_REG(0x16, 0x20);
    TYHX_Write_REG(0x20, 0x88);
    TYHX_Write_REG(0x21, 0xD3);
    TYHX_Write_REG(0x00, 0x0A);//0x0E : INT OD mode; 0x0A : INT CMOS mode
}


void hx300x_int_handle(void)
{
    uint16_t ps_data = 0;
//方法一：直接读INT的高低电平  （推荐方法）
    // INT 为高电平 ===> 入耳 （In ear）
    // INT 为低电平 ===> 出耳 （Out ear）

//方法二：读ps data 和高低阈值对比
    /*
        ps_data = hx300x_read_ps_data();
        //printf("PS Data=%d\r\n",ps_data);
        if(ps_data > H_THOD)
          {
               //in_ear_flag = 1;                       // In ear
               //SEGGER_RTT_printf(0,"*************************************hx3002 In ear \r\n");
          }
        else if(ps_data < L_THOD)
         {
              //in_ear_flag = 0;                     // Out ear
              //SEGGER_RTT_printf(0,"=====================================hx3002 Out ear \r\n");
         }
    */
}

void hx300x_PowerDown(void)
{
    // Or disable EN of LDO
    TYHX_Write_REG(0x00, 0x05);	 		                                //bit1:PEN  关闭激光电源
}

uint16_t hx300x_read_ps_data(void)
{
    uint8_t ps_data_h = 0;
    uint8_t ps_data_l = 0;
    uint16_t ps_dataTemp1 = 0;

    ps_data_h = TYHX_Read_REG(0x08);
    ps_data_l = TYHX_Read_REG(0x07);

    ps_dataTemp1 = (ps_data_h << 8) + ps_data_l;
    printf("hx3002 str data=%d \r\n", ps_dataTemp1);
    return ps_dataTemp1;
}
//=============================================================================================
uint8_t Calibration_First_IIC_INT_check(void)
{
#if 1
    uint8_t cnt = 0;
    uint8_t Reg[2];
//
    TYHX_Write_REG(0x00, 0x01);
    TYHX_Write_REG(0x01, 0x12);
    hx300x_Set_Thres(0, 0);
    TYHX_Write_REG(0x00, 0x02);
    os_time_dly(10);

//IIC Test
    printf("\r\n...IIC 测试开始......\r\n");

    Reg[0] = TYHX_Read_REG(0x01);
    Reg[1] = TYHX_Read_REG(0x00);
    if ((Reg[0] == 0x12) && (Reg[1] == 0x02)) {
        printf("\r\n...IIC 通讯OK...\r\n");
    } else {
        printf("\r\n　!!! IIC 通讯失败...\r\n");
        return 3; //0:IIC 通讯失败
    }

    printf("reg[04]=0x%x, reg[05]=0x%x, reg[06]=0x%x\r\n", TYHX_Read_REG(0x04), TYHX_Read_REG(0x05), TYHX_Read_REG(0x06));
    printf("\r\n...INT 测试开始......\r\n");
    while (1) {
        printf("INT_check step 1 cnt=%d \r\n", cnt);
        if (0 == gpio_read(IO_PORTC_03)) {
            printf("INT_check level1 pass\r\n");
            break;
        }
        cnt++;
        if (cnt > 20) {
            printf("INT_check level1 failed\r\n");
            return 2;		// 2: INT 失败
        }
        os_time_dly(1);
    }
    os_time_dly(4);
    cnt = 0;
    while (1) {
        printf("INT_check step 2 cnt=%d \r\n", cnt);
        hx300x_read_ps_data();
        os_time_dly(1);
        if (1 == gpio_read(IO_PORTC_03)) {
            printf("INT_check level2 pass\r\n");
            printf("\r\n");
            printf("\r\n...INT 测试OK...\r\n");
            return 1;   //1: IIC + INT 都通过
        }
        cnt++;
        if (cnt > 20) {
            printf("\r\n　!!! INT 测试失败...\r\n");
            return 2;		// 2: INT 失败
        }
    }
    printf("\r\n　!!! INT 测试失败...\r\n");
    return 2;		// 2: INT 失败
#endif
}

eartouch_state hx300x_ir_check(void *priv)
{
    if (gpio_read(platform_data->int_port)) {
        return EARTOUCH_STATE_IN_EAR;                           //入耳
    } else {
        return EARTOUCH_STATE_OUT_EAR;                          //出耳
    }
}

//IR 初始化函数
u8 hx300x_ir_sensor_init(eartouch_platform_data  *cfg)
{
    int retval = 0;
    g_printf("%s\n", __func__);
    platform_data = cfg;
    retval = irSensor_iic_init(cfg);
    if (retval < 0) {
        y_printf("\n  open iic for gsensor err\n");
        return retval;
    } else {
        y_printf("\n iic open succ\n");
    }

    //光感模块初始化
    HX300x_init();
    return 0;

}

REGISTER_EARTOUCH(hx300x) = {
    .logo = "hx300x",
    .interrupt_mode = EARTCH_INTERRUPT_MODE,
    .int_level = 1,
    .init = hx300x_ir_sensor_init,
    .check = hx300x_ir_check,
};

#endif

