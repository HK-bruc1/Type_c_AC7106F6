#ifndef __P33_API_H__
#define __P33_API_H__


//
//
//					vol
//
//
//
/****************************************************************/
enum DVDD_VOL {
    DVDD_VOL_084V = 0,
    DVDD_VOL_087V,
    DVDD_VOL_090V,
    DVDD_VOL_093V,
    DVDD_VOL_096V,
    DVDD_VOL_099V,
    DVDD_VOL_102V,
    DVDD_VOL_105V,
    DVDD_VOL_108V,
    DVDD_VOL_111V,
    DVDD_VOL_114V,
    DVDD_VOL_117V,
    DVDD_VOL_120V,
    DVDD_VOL_123V,
    DVDD_VOL_126V,
    DVDD_VOL_129V,
};

/*
enum DVDD2_VOL {
    DVDD2_VOL_0675V = 0,
    DVDD2_VOL_0700V,
    DVDD2_VOL_0725V,
    DVDD2_VOL_0750V,
    DVDD2_VOL_0775V,
    DVDD2_VOL_0800V,
    DVDD2_VOL_0825V,
    DVDD2_VOL_0850V,
    DVDD2_VOL_0875V,
    DVDD2_VOL_0900V,
    DVDD2_VOL_0925V,
    DVDD2_VOL_0950V,
    DVDD2_VOL_0975V,
    DVDD2_VOL_1000V,
    DVDD2_VOL_1025V,
    DVDD2_VOL_1050V,
};
*/
enum RVDD_VOL {
    RVDD_VOL_084V = 0,
    RVDD_VOL_087V,
    RVDD_VOL_090V,
    RVDD_VOL_093V,
    RVDD_VOL_096V,
    RVDD_VOL_099V,
    RVDD_VOL_102V,
    RVDD_VOL_105V,
    RVDD_VOL_108V,
    RVDD_VOL_111V,
    RVDD_VOL_114V,
    RVDD_VOL_117V,
    RVDD_VOL_120V,
    RVDD_VOL_123V,
    RVDD_VOL_126V,
    RVDD_VOL_129V,
};

/*
enum RVDD2_VOL {
    RVDD2_VOL_0675V = 0,
    RVDD2_VOL_0700V,
    RVDD2_VOL_0725V,
    RVDD2_VOL_0750V,
    RVDD2_VOL_0775V,
    RVDD2_VOL_0800V,
    RVDD2_VOL_0825V,
    RVDD2_VOL_0850V,
    RVDD2_VOL_0875V,
    RVDD2_VOL_0900V,
    RVDD2_VOL_0925V,
    RVDD2_VOL_0950V,
    RVDD2_VOL_0975V,
    RVDD2_VOL_1000V,
    RVDD2_VOL_1025V,
    RVDD2_VOL_1500V,
};

enum BTVDD_VOL {
    BTVDD_VOL_1625V = 0,
    BTVDD_VOL_1650V,
    BTVDD_VOL_1675V,
    BTVDD_VOL_1700V,
    BTVDD_VOL_1725V,
    BTVDD_VOL_1750V,
    BTVDD_VOL_1775V,
    BTVDD_VOL_1800V,
    BTVDD_VOL_1825V,
    BTVDD_VOL_1850V,
    BTVDD_VOL_1900V,
    BTVDD_VOL_1925V,
    BTVDD_VOL_1950V,
    BTVDD_VOL_1975V,
    BTVDD_VOL_2000V,
};
*/
enum DCVDD_VOL {
    DCVDD_VOL_100V = 0,
    DCVDD_VOL_105V,
    DCVDD_VOL_110V,
    DCVDD_VOL_115V,
    DCVDD_VOL_120V,
    DCVDD_VOL_125V,
    DCVDD_VOL_130V,
    DCVDD_VOL_135V,
    DCVDD_VOL_140V,
    DCVDD_VOL_145V,
    DCVDD_VOL_150V,
    DCVDD_VOL_155V,
    DCVDD_VOL_160V,
    DCVDD_VOL_160VV,
    DCVDD_VOL_160VVV,
    DCVDD_VOL_160VVVV,
};

enum VDDIOM_VOL {
    VDDIOM_VOL_21V = 0,
    VDDIOM_VOL_22V,
    VDDIOM_VOL_23V,
    VDDIOM_VOL_24V,
    VDDIOM_VOL_25V,
    VDDIOM_VOL_26V,
    VDDIOM_VOL_27V,
    VDDIOM_VOL_28V,
    VDDIOM_VOL_29V,
    VDDIOM_VOL_30V,
    VDDIOM_VOL_31V,
    VDDIOM_VOL_32V,
    VDDIOM_VOL_33V,
    VDDIOM_VOL_34V,
    VDDIOM_VOL_35V,
    VDDIOM_VOL_36V,
};

enum VDDIOW_VOL {
    VDDIOW_VOL_21V = 0,
    VDDIOW_VOL_22V,
    VDDIOW_VOL_23V,
    VDDIOW_VOL_24V,
    VDDIOW_VOL_25V,
    VDDIOW_VOL_26V,
    VDDIOW_VOL_27V,
    VDDIOW_VOL_28V,
    VDDIOW_VOL_29V,
    VDDIOW_VOL_30V,
    VDDIOW_VOL_31V,
    VDDIOW_VOL_32V,
    VDDIOW_VOL_33V,
    VDDIOW_VOL_34V,
    VDDIOW_VOL_35V,
    VDDIOW_VOL_36V,
};

enum WVDD_VOL {
    WVDD_VOL_050V = 0,
    WVDD_VOL_055V,
    WVDD_VOL_060V,
    WVDD_VOL_065V,
    WVDD_VOL_070V,
    WVDD_VOL_075V,
    WVDD_VOL_080V,
    WVDD_VOL_085V,
    WVDD_VOL_090V,
    WVDD_VOL_095V,
    WVDD_VOL_100V,
    WVDD_VOL_105V,
    WVDD_VOL_110V,
    WVDD_VOL_115V,
    WVDD_VOL_120V,
    WVDD_VOL_125V,
};

enum PVDD_VOL {
    PVDD_VOL_050V = 0,
    PVDD_VOL_055V,
    PVDD_VOL_060V,
    PVDD_VOL_065V,
    PVDD_VOL_070V,
    PVDD_VOL_075V,
    PVDD_VOL_080V,
    PVDD_VOL_085V,
    PVDD_VOL_090V,
    PVDD_VOL_095V,
    PVDD_VOL_100V,
    PVDD_VOL_105V,
    PVDD_VOL_110V,
    PVDD_VOL_115V,
    PVDD_VOL_120V,
    PVDD_VOL_125V,
};

void dvdd_vol_sel(enum DVDD_VOL vol);
enum DVDD_VOL get_dvdd_vol_sel();
u8 dvdd_lev_to_wvdd_lev(u8 dvdd_lev);

//void dvdd2_vol_sel(enum DVDD2_VOL vol);
//enum DVDD2_VOL get_dvdd2_vol_sel();

void rvdd_vol_sel(enum RVDD_VOL vol);
enum RVDD_VOL get_rvdd_vol_sel();
//void rvdd2_vol_sel(enum RVDD2_VOL vol);
//enum RVDD2_VOL get_rvdd2_vol_sel();

void dcvdd_vol_max_sel(enum DCVDD_VOL vol);
enum DCVDD_VOL get_dcvdd_vol_max_sel();

void dcvdd_vol_min_sel(enum DCVDD_VOL vol);
enum DCVDD_VOL get_dcvdd_vol_min_sel();

void dcvdd_vol_sel(enum DCVDD_VOL vol);
enum DCVDD_VOL get_dcvdd_vol_sel();

void dcvdd_ctl_audio_to_p33(void);
void dcvdd_ctl_p33_to_audio(void);
u32 dcvdd_is_audio_ctl(void);

//void btvdd_vol_sel(enum BTVDD_VOL vol);
//enum BTVDD_VOL get_btvdd_vol_sel();

void pvdd_config(u32 lev, u32 low_lev, u32 output);
void pvdd_output(u32 output);

void vddiom_vol_sel(enum VDDIOM_VOL vol);
enum VDDIOM_VOL get_vddiom_vol_sel();
void vddiow_vol_sel(enum VDDIOW_VOL vol);
enum VDDIOW_VOL get_vddiow_vol_sel();
u32 get_vddiom_vol();

//
//
//					lvd
//
//
//
/****************************************************************/
typedef enum {
    LVD_RESET_MODE,		//复位模式
    LVD_EXCEPTION_MODE, //异常模式，进入异常中断
    LVD_WAKEUP_MODE,    //唤醒模式，进入唤醒中断，callback参数为回调函数
} LVD_MODE;

typedef enum {
    VLVD_SEL_1806MV = 0,
    VLVD_SEL_1906MV,
    VLVD_SEL_2006MV,
    VLVD_SEL_2106MV,
    VLVD_SEL_2206MV,
    VLVD_SEL_2306MV,
    VLVD_SEL_2407MV,
    VLVD_SEL_2505MV,
} LVD_VOL;

void lvd_en(u8 en);
void lvd_config(LVD_VOL vol, u8 expin_en, LVD_MODE mode, void (*callback));

//
//
//                    dcdc
//
//
//
//******************************************************************
enum POWER_MODE {
    //LDO模式
    PWR_LDO15,
    //DCDC模式
    PWR_DCDC15,
};

enum POWER_DCDC_TYPE {
    PWR_DCDC12 = 2,
    PWR_DCDC12_DCDC09 = 3,
};

enum {
    DCDC09 = 1,
    DCDC12 = 2,
    DCDC18 = 4,
};

void power_set_dcdc_type(enum POWER_DCDC_TYPE type);
void power_set_mode(enum POWER_MODE mode);

#define MAX_WAKEUP_PORT     8  //最大同时支持数字io输入个数
#define MAX_WAKEUP_ANA_PORT 3  //最大同时支持模拟io输入个数

#endif

