#include "app_config.h"
#include "hr_sensor/hrSensor_manage.h"
#include "gSensor/gSensor_manage.h"
#include "hx3918.h"

//#include "tyhx_hrs_custom.h"
#ifdef SAR_ALG_LIB
#include "hx3918_prox.h"
#endif

#ifdef HRS_ALG_LIB
#include "hx3918_hrs_agc.h"
#include "tyhx_hrs_alg.h"
#endif

#ifdef SPO2_ALG_LIB
#include "hx3918_spo2_agc.h"
#include "tyhx_spo2_alg.h"
#endif

#ifdef HRV_ALG_LIB
#include "hx3918_hrv_agc.h"
#include "tyhx_hrv_alg.h"
#endif

#ifdef CHECK_TOUCH_LIB
#include "hx3918_check_touch.h"
#endif
#include "hx3918_factory_test.h"


#if TCFG_HRSENSOR_ENABLE && TCFG_HX3918_ENABLE

#ifdef SPO2_VECTOR
#include "spo2_vec.h"
uint32_t spo2_send_cnt = 0;
int32_t red_buf_vec[8];
int32_t ir_buf_vec[8];
int32_t green_buf_vec[8];
#endif

#ifdef HR_VECTOR
#include "hr_vec.h"
uint32_t hrs_send_cnt = 0;
int32_t PPG_buf_vec[8];
#endif

#ifdef HRV_TESTVEC
#include "hrv_testvec.h"
#endif

tyhx_hrsresult_t hrsresult;

#ifdef GSENSER_DATA

#define GSENSER_CBUF_SIZE  (64*2*3)
static cbuffer_t *gsensor_cbuf = NULL;
volatile int16_t gsen_fifo_x[64];  //ppg time 330ms..330/40 = 8.25
volatile int16_t gsen_fifo_y[64];
volatile int16_t gsen_fifo_z[64];
#else
int16_t gen_dummy[64] = {0};
#endif
//SPO2 agc
const uint8_t  hx3918_spo2_agc_green_idac = GREEN_AGC_OFFSET;
const uint8_t  hx3918_spo2_agc_red_idac = RED_AGC_OFFSET;
const uint8_t  hx3918_spo2_agc_ir_idac = IR_AGC_OFFSET;
//hrs agc
const uint8_t  hx3918_hrs_agc_idac = HR_GREEN_AGC_OFFSET;
//hrv agc
const uint8_t  hx3918_hrv_agc_idac = HR_GREEN_AGC_OFFSET;

const uint8_t  red_led_max_init = 200;
const uint8_t  ir_led_max_init = 200;
const uint8_t  green_led_max_init = 150;

uint8_t low_vbat_flag = 0;


const uint16_t static_thre_val = 150;
const uint8_t  gsen_lv_val = 0;


//HRS_INFRARED_THRES
const int32_t  hrs_ir_unwear_thres = 3000;
const int32_t  hrs_ir_wear_thres = 3500;
//HRV_INFRARED_THRES
const int32_t  hrv_ir_unwear_thres = 3000;
const int32_t  hrv_ir_wear_thres = 3500;
//SPO2_INFRARED_THRES
const uint8_t  spo2_check_unwear_oft = 4;
const int32_t  spo2_ir_unwear_thres = 250000;
const int32_t  spo2_ir_wear_thres = 10000;
//CHECK_WEAR_MODE_THRES
const int32_t  check_mode_unwear_thre = 3000;
const int32_t  check_mode_wear_thre = 3500;
//const int32_t  check_mode_sar_thre = 6000;

int32_t  sar_wear_thres = 3000;  // 可设定为正常的1/3, 必须小于32767
int32_t  sar_unwear_thres = 2400; // 可设定为 sar_wear_thres *0.8

uint8_t alg_ram[5 * 1024] __attribute__((aligned(4)));
ppg_sensor_data_t ppg_s_dat;
uint8_t read_fifo_first_flg = 0;
hrs_sports_mode_t hrs_mode = NORMAL_MODE;



//////// spo2 para and switches
const  uint8_t   COUNT_BLOCK_NUM = 50;            //delay the block of some single good signal after a series of bad signal
const  uint8_t   SPO2_LOW_XCORR_THRE = 30;        //(64*xcorr)'s square below this threshold, means error signal
const  uint8_t   SPO2_CALI = 1;                       //spo2_result cali mode
const  uint8_t   XCORR_MODE = 1;                  //xcorr mode switch
const  uint8_t   QUICK_RESULT = 1;                //come out the spo2 result quickly ;0 is normal,1 is quick
const  uint16_t  MEAN_NUM = 32;                  //the length of smooth-average ;the value of MEAN_NUM can be given only 256 and 512
const  uint8_t   G_SENSOR = 0;                      //if =1, open the gsensor mode
const  uint8_t   SPO2_GSEN_POW_THRE = 150;         //gsen pow judge move, range:0-200;
const  uint32_t  SPO2_BASE_LINE_INIT = 163000;    //spo2 baseline init, = 103000 + ratio(a variable quantity,depends on different cases)*SPO2_SLOPE
const  int32_t   SOP2_DEGLITCH_THRE = 5000;     //remove signal glitch over this threshold
const  int32_t   SPO2_REMOVE_JUMP_THRE = 5000;  //remove signal jump over this threshold
const  uint32_t  SPO2_SLOPE = 50000;              //increase this slope, spo2 reduce more
const  uint16_t  SPO2_LOW_CLIP_END_TIME = 1500;   //low clip mode before this data_cnt, normal clip mode after this
const  uint16_t  SPO2_LOW_CLIP_DN  = 150;         //spo2 reduce 0.15/s at most in low clip mode
const  uint16_t  SPO2_NORMAL_CLIP_DN  = 500;      //spo2 reduce 0.5/s at most in normal clip mode
const  uint8_t   SPO2_LOW_SNR_THRE = 40;          //snr below this threshold, means error signal
const  uint16_t  IR_AC_TOUCH_THRE = 200;          //AC_min*0.3
const  uint16_t  IR_FFT_POW_THRE = 200;           //fft_pow_min
const  uint8_t   SLOPE_PARA_MAX = 37;
const  uint8_t   SLOPE_PARA_MIN = 3;

WORK_MODE_T work_mode_flag = HRV_MODE;

void hx3918_delay(uint32_t ms)
{
    mdelay(ms);
}

//Timer interrupt 40ms repeat mode
void gsen_read_timeout_handler(void *p_context)
{
    hx3918_agc_Int_handle();

#ifdef LAB_TEST_INT_MODE
    if (work_mode_flag == LAB_TEST_MODE) { // add ericy 20210127
        hx3690l_lab_test_read();
    }
#endif
}

void gsensor_write_cbuf(short frame, short *buf)
{
    u32 w_size = 2 * 3 * frame;

    if (gsensor_cbuf == NULL) {
        return ;
    }

    //如果cbuf满了丢掉旧数据，写入新数据，以保证数据最新
    if (!cbuf_is_write_able(gsensor_cbuf, w_size)) {
        cbuf_read_alloc_len_updata(gsensor_cbuf, w_size);
    }

    w_size = cbuf_write(gsensor_cbuf, buf, w_size);
    if (w_size == 0) {
        printf("data_w_cbuf_full");
    }
}

void gsensor_read_cbuf(void)
{
    axis_info_t gsensor_data[32];
    u8 i;

    u8 gsen_frame =  cbuf_read_alloc_len(gsensor_cbuf, gsensor_data, sizeof(gsensor_data)) / 6;

    for (i = 0; i < gsen_frame; i++) {
        gsen_fifo_x[i] = gsensor_data[i].x;
        gsen_fifo_y[i] = gsensor_data[i].y;
        gsen_fifo_z[i] = gsensor_data[i].z;
    }
    // // 数据不够补最后一笔
    // if(gsen_frame < 32){
    //     for(; i<32; i++){
    //         gsen_fifo_x[i] = gsensor_data[gsen_frame-1].x;
    //         gsen_fifo_y[i] = gsensor_data[gsen_frame-1].y;
    //         gsen_fifo_z[i] = gsensor_data[gsen_frame-1].z;
    //     }
    // }
}


bool hx3918_write_reg(uint8_t addr, uint8_t data)
{
    if (hrsensor_write_nbyte(0x88, addr, &data, 1) != 1) {
        log_e("hx3918_write_reg error! addr=%02x data=%02x\n", addr, data);
    }
    hx3918_delay(1);
    return true;
}

uint8_t hx3918_read_reg(uint8_t addr)
{
    uint8_t data_buf = 0;
    if (hrsensor_read_nbyte(0x89, addr, &data_buf, 1) != 1) {
        log_e("hx3918_read_reg error! addr=%02x\n", addr);
    }
    return data_buf;
}

bool hx3918_brust_read_reg(uint8_t addr, uint8_t *buf, uint8_t length)
{
    if (hrsensor_read_nbyte(0x89, addr, buf, length) != length) {
        log_e("hx3918_brust_read_reg error! addr=%02x length=%02x\n", addr, length);
    }
    return true;
}

/*320ms void hx3918_ppg_Int_handle(void)*/
void hx3918_ppg_timer_cfg(bool en)    //320ms
{
    static u16 timer_handle = 0;

    if (en && timer_handle == 0) {
        timer_handle = sys_timer_add(NULL, heart_rate_meas_timeout_handler, 320);
    } else if ((!en) &&  timer_handle > 0) {
        sys_timeout_del(timer_handle);
        timer_handle = 0;
    }
    TYHX_LOG("hx3918_ppg_timer_cfg %d handle=%d\n", en, timer_handle);
}

/* 40ms void agc_timeout_handler(void * p_context) */
void hx3918_agc_timer_cfg(bool en)
{
    static u16 timer_handle = 0;

    if (en && timer_handle == 0) {
        timer_handle = sys_timer_add(NULL, gsen_read_timeout_handler, 40);
    } else if ((!en) &&  timer_handle > 0) {
        sys_timeout_del(timer_handle);
        timer_handle = 0;
    }
    TYHX_LOG("hx3918_agc_timer_cfg %d handle=%d\n", en, timer_handle);

}
#if defined(INT_MODE)
void hx3918_gpioint_cfg(bool en)
{
    if (en) {
        hx3918_gpioint_enable();
    } else {
        hx3918_gpioint_disable();
    }

}
#endif

uint8_t hx3918_chip_id = 0;
bool hx3918_chip_check(void)
{
    uint8_t i = 0;

    for (i = 0; i < 10; i++) {
        hx3918_write_reg(0x01, 0x00);
        // hx3918_delay(5);
        hx3918_chip_id = hx3918_read_reg(0x00);
        if (hx3918_chip_id == HX3918_ID || hx3918_chip_id == HX3917_ID) {
            TYHX_LOG("chip id  = 0x%x	check ok \r\n", hx3918_chip_id);
            return true;
        }
    }
    TYHX_LOG("hx3918_chip_check fail\r\n");
    return false;
}

uint8_t hx3918_read_fifo_size(void)
{
    uint8_t fifo_num_temp = 0;
    fifo_num_temp = hx3918_read_reg(0x40) & 0x7f; // ericy 230529

    return fifo_num_temp;
}
uint16_t hx3918_read_fifo_data(int32_t *buf, uint8_t phase_num, uint8_t sig)
{
    uint8_t data_flg_start = 0;
    uint8_t data_flg_end = 0;
    uint8_t databuf[3];
    uint32_t ii = 0;
    uint16_t data_len = 0;
    uint16_t fifo_data_length = 0;
    uint16_t fifo_read_length = 0;
    uint16_t fifo_read_bytes = 0;
    uint16_t fifo_out_length = 0;
    uint16_t fifo_out_count = 0;
    uint8_t fifo_data_buf[200] = {0};

    data_len = hx3918_read_reg(0x40);// ericy 230529
    fifo_data_length = data_len;
    //DEBUG_PRINTF("data_len:   %d\r\n",data_len);
    if (fifo_data_length < 2 * phase_num) {
        return 0;
    }
    fifo_read_length = ((fifo_data_length - phase_num) / phase_num) * phase_num;
    fifo_read_bytes = fifo_read_length * 3;
    if (read_fifo_first_flg == 1) {
        hx3918_brust_read_reg(0x41, databuf, 3);	// ericy 230529
        read_fifo_first_flg = 0;
    }
    hx3918_brust_read_reg(0x41, fifo_data_buf, fifo_read_bytes);	// ericy 230529
//		for(ii=0; ii<fifo_read_bytes; ii++)
//		{
//			DEBUG_PRINTF("%d/%d, %d\r\n", ii+1,fifo_read_bytes,fifo_data_buf[ii]);
//		}
    for (ii = 0; ii < fifo_read_length; ii++) {
        if (sig == 0) {
            buf[ii] = (int32_t)(fifo_data_buf[ii * 3] | (fifo_data_buf[ii * 3 + 1] << 8) | ((fifo_data_buf[ii * 3 + 2] & 0x1f) << 16));
        } else {
            if ((fifo_data_buf[ii * 3 + 2] & 0x10) != 0) {
                buf[ii] = (int32_t)(fifo_data_buf[ii * 3] | (fifo_data_buf[ii * 3 + 1] << 8) | ((fifo_data_buf[ii * 3 + 2] & 0x0f) << 16)) - 1048576;
            } else {
                buf[ii] = (int32_t)(fifo_data_buf[ii * 3] | (fifo_data_buf[ii * 3 + 1] << 8) | ((fifo_data_buf[ii * 3 + 2] & 0x1f) << 16));
            }
        }
        //DEBUG_PRINTF("%d/%d, %d %d\r\n", ii+1,fifo_read_length,buf[ii],(fifo_data_buf[ii*3+2]>>5)&0x03);
    }
    data_flg_start = (fifo_data_buf[2] >> 5) & 0x03;
    data_flg_end = (fifo_data_buf[fifo_read_bytes - 1] >> 5) & 0x03;
    fifo_out_length = fifo_read_length;
    if (data_flg_start > 0) {
        fifo_out_length = fifo_read_length - phase_num + data_flg_start;
        for (ii = 0; ii < fifo_out_length; ii++) {
            buf[ii] = buf[ii + phase_num - data_flg_start];
        }
        for (ii = fifo_out_length; ii < fifo_read_length; ii++) {
            buf[ii] = 0;
        }
    }
    if (data_flg_end < phase_num - 1) {
        for (ii = fifo_out_length; ii < fifo_out_length + phase_num - data_flg_end - 1; ii++) {
            hx3918_brust_read_reg(0x41, databuf, 3);    // ericy 230529
        }
        buf[ii] = (int32_t)(databuf[0] | (databuf[1] << 8) | ((databuf[2] & 0x1f) << 16));
    }
//		for(ii=0; ii<fifo_out_length; ii++)
//		{
//			DEBUG_PRINTF("%d/%d, %d\r\n", ii+1,fifo_out_length,buf[ii]);
//		}
    fifo_out_length = fifo_out_length + phase_num - data_flg_end - 1;
    fifo_out_count = fifo_out_length / phase_num;
    if (data_len == 64) {
        uint8_t reg_0x2d = hx3918_read_reg(0x3d);// ericy 230529
        hx3918_write_reg(0x3d, 0x00); // ericy 230529
        hx3918_delay(5);
        hx3918_write_reg(0x3d, reg_0x2d); // ericy 230529
        read_fifo_first_flg = 1;
    }
    return fifo_out_count;
}

void read_data_packet(int32_t *ps_data)  // 3918
{
    uint8_t  databuf1[6] = {0};
    uint8_t  databuf2[6] = {0};
    hx3918_brust_read_reg(0x02, databuf1, 6);
    hx3918_brust_read_reg(0x08, databuf2, 6);

    ps_data[0] = ((databuf1[0]) | (databuf1[1] << 8) | (databuf1[2] << 16));
    ps_data[1] = ((databuf1[3]) | (databuf1[4] << 8) | (databuf1[5] << 16));
    ps_data[2] = ((databuf2[0]) | (databuf2[1] << 8) | (databuf2[2] << 16));
    ps_data[3] = ((databuf2[3]) | (databuf2[4] << 8) | (databuf2[5] << 16));
//    DEBUG_PRINTF(" %d %d %d %d \r\n",  ps_data[0], ps_data[1], ps_data[2], ps_data[3]);
}



void hx3918_vin_check(uint16_t led_vin)
{
    low_vbat_flag = 0;
    if (led_vin < 3700) {
        low_vbat_flag = 1;
    }
}

void hx3918_ppg_off(void)
{
#ifdef SAR_ALG_LIB
    hx3918_write_reg(0x6a, 0x00); // 增加sar 寄存器作为sar 是否开启判断
#endif

    hx3918_write_reg(0x2a, 0x00);
    hx3918_write_reg(0x2b, 0x00);
    hx3918_write_reg(0x16, 0x00);
    hx3918_write_reg(0x01, 0x01);
}

void hx3918_ppg_on(void)
{
    hx3918_write_reg(0x01, 0x00);
    hx3918_delay(5);
}

void hx3918_data_reset(void)
{
#if defined(TIMMER_MODE)
    hx3918_ppg_timer_cfg(false);
    hx3918_agc_timer_cfg(false);
#elif defined(INT_MODE)
    hx3918_gpioint_cfg(false);
#endif
#if defined(HRS_ALG_LIB)
    hx3918_hrs_data_reset();
#endif
#if defined(SPO2_ALG_LIB)
    hx3918_spo2_data_reset();
#endif
#if defined(HRV_ALG_LIB)
    hx3918_hrv_data_reset();
#endif
    TYHX_LOG("hx3918 data reset!\r\n");
}

void Efuse_Mode_Check(void)
{
    uint8_t  REG_45_E, REG_46_E, REG_47_E, REG_48_E;

    uint8_t chip_vision_check = 0;
    chip_vision_check = hx3918_read_reg(0x47) >> 6;

    if (chip_vision_check > 0) {
        return;
    }

    hx3918_write_reg(0x4b, 0x10);
    hx3918_write_reg(0x4d, 0x18);
    hx3918_write_reg(0x4e, 0x40);
    hx3918_write_reg(0x4C, 0x04);
    hx3918_write_reg(0x4C, 0x00);

    REG_45_E = hx3918_read_reg(0x45);
    REG_46_E = hx3918_read_reg(0x46);
    REG_47_E = hx3918_read_reg(0x47);
    REG_48_E = hx3918_read_reg(0x48);

    hx3918_write_reg(0X4B, 0x20);
    hx3918_write_reg(0x45, REG_45_E);
    hx3918_write_reg(0x46, REG_46_E);
    hx3918_write_reg(0x47, REG_47_E);
    hx3918_write_reg(0x48, REG_48_E);
    hx3918_write_reg(0X4B, 0x00);

    hx3918_write_reg(0x4d, 0x20);
    hx3918_write_reg(0x4e, 0x00);
    hx3918_write_reg(0x4C, 0x00);

}

bool hx3918_init(WORK_MODE_T mode)
{
    work_mode_flag = mode;
    hx3918_data_reset();
    hx3918_vin_check(3800);
    hx3918_ppg_on();
    Efuse_Mode_Check();
    switch (work_mode_flag) {
    case HRS_MODE:
        tyhx_hrs_set_living(0, 0, 0); //  qu=15  std=30
        if (hx3918_hrs_enable() == SENSOR_OP_FAILED) {
            return false;
        }
        break;

    case LIVING_MODE:
#ifdef CHECK_LIVING_LIB
        if (hx3918_hrs_enable() == SENSOR_OP_FAILED) {
            return false;
        }
#endif
        break;

    case SPO2_MODE:
#ifdef SPO2_DATA_CALI
        hx3918_spo2_data_cali_init();
#endif
#ifdef SPO2_ALG_LIB
        tyhx_spo2_para_usuallyadjust(SPO2_LOW_XCORR_THRE, SPO2_LOW_SNR_THRE, COUNT_BLOCK_NUM, SPO2_BASE_LINE_INIT, SPO2_SLOPE, SPO2_GSEN_POW_THRE);
        tyhx_spo2_para_barelychange(MEAN_NUM, SOP2_DEGLITCH_THRE, SPO2_REMOVE_JUMP_THRE, SPO2_LOW_CLIP_END_TIME, SPO2_LOW_CLIP_DN, \
                                    SPO2_NORMAL_CLIP_DN, IR_AC_TOUCH_THRE, IR_FFT_POW_THRE, SPO2_CALI, SLOPE_PARA_MAX, SLOPE_PARA_MIN);
        if (hx3918_spo2_enable() == SENSOR_OP_FAILED) {
            return false;
        }
#endif
        break;

    case HRV_MODE:
#ifdef HRV_ALG_LIB
        if (hx3918_hrv_enable() == SENSOR_OP_FAILED) {
            return false;
        }

        break;
#endif
    case WEAR_MODE:
#ifdef CHECK_TOUCH_LIB

        if (hx3918_check_touch_enable() == SENSOR_OP_FAILED) {
            return false;
        }
#endif
        break;

    case FT_LEAK_LIGHT_MODE:
        if (!hx3918_chip_check()) {
            AGC_LOG("check id failed!\r\n");
            return false;
        }
        hx3918_factroy_test(LEAK_LIGHT_TEST);
        break;

    case FT_GRAY_CARD_MODE:
        hx3918_factroy_test(GRAY_CARD_TEST);
        break;

    case FT_INT_TEST_MODE:
        hx3918_factroy_test(FT_INT_TEST);
//			  hx3918_gpioint_cfg(true);
        break;

    case LAB_TEST_MODE:
#ifdef LAB_TEST
        if (hx3918_lab_test_enable() == SENSOR_OP_FAILED) {
            return false;
        }
#endif
        break;

    default:
        break;
    }

    return true;
}

void hx3918_agc_Int_handle(void)
{

    switch (work_mode_flag) {
    case HRS_MODE: {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if (cal.work) {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case LIVING_MODE: {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if (cal.work) {
            //TYHX_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case HRV_MODE: {
#ifdef HRV_ALG_LIB
        HRV_CAL_SET_T cal;
        cal = PPG_hrv_agc();
        if (cal.work) {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case SPO2_MODE: {
#ifdef SPO2_ALG_LIB
        SPO2_CAL_SET_T cal;
        cal = PPG_spo2_agc();
        if (cal.work) {
            //AGC_LOG("INT_AGC: Rled_drv=%d,Irled_drv=%d,RledDac=%d,IrledDac=%d,ambDac=%d,Rledstep=%d,Irledstep=%d,Rrf=%d,Irrf=%d,\r\n",\
            //cal.red_cur, cal.ir_cur,cal.red_idac,cal.ir_idac,cal.amb_idac,cal.red_led_step,cal.red_led_step,cal.ontime,cal.state);
        }
#endif
        break;
    }
    case LAB_TEST_MODE:
#ifdef LAB_TEST
        hx3918_lab_test_Int_handle();
#endif
    default:
        break;
    }
}


void agc_timeout_handler(void *p_context)
{
#ifdef TIMMER_MODE
    switch (work_mode_flag) {
    case HRS_MODE: {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if (cal.work) {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case LIVING_MODE: {
#ifdef HRS_ALG_LIB
        HRS_CAL_SET_T cal;
        cal = PPG_hrs_agc();
        if (cal.work) {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case HRV_MODE: {
#ifdef HRV_ALG_LIB
        HRV_CAL_SET_T cal;
        cal = PPG_hrv_agc();
        if (cal.work) {
            //AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,rf=%d\r\n", \
            //cal.led_cur, cal.led_idac, cal.amb_idac,cal.led_step,cal.ontime);
        }
#endif
        break;
    }
    case SPO2_MODE: {
#ifdef SPO2_ALG_LIB
        SPO2_CAL_SET_T cal;
        cal = PPG_spo2_agc();
        if (cal.work) {
            //AGC_LOG("AGC: Rled_drv=%d,Irled_drv=%d,RledDac=%d,IrledDac=%d,ambDac=%d,Rledstep=%d,Irledstep=%d,Rrf=%d,Irrf=%d,\r\n", \
            //cal.red_cur, cal.ir_cur,cal.red_idac,cal.ir_idac,cal.amb_idac,cal.red_led_step,cal.red_led_step,cal.ontime,cal.state);
        }
#endif
        break;
    }
    case LAB_TEST_MODE: {
#ifdef LAB_TEST
        hx3918_lab_test_Int_handle();
#endif
    }
    default:
        break;
    }
#endif
}

//void ppg_timeout_handler(void * p_context)
void heart_rate_meas_timeout_handler(void *p_context)
{
    hx3918_ppg_Int_handle();
}

void hx3918_ppg_Int_handle(void)
{
#if defined(INT_MODE)
    hx3918_agc_Int_handle();
#endif
#ifdef SAR_ALG_LIB
    hx3918_report_prox_status();
#endif

    switch (work_mode_flag) {
    case HRS_MODE:
#ifdef HRS_ALG_LIB
        hx3918_hrs_ppg_Int_handle();
#endif

        break;

    case SPO2_MODE:
#ifdef SPO2_ALG_LIB
        hx3918_spo2_ppg_Int_handle();
#endif
        break;

    case HRV_MODE:
#ifdef HRV_ALG_LIB
        hx3918_hrv_ppg_Int_handle();
#endif
        break;

    case WEAR_MODE:
#ifdef CHECK_TOUCH_LIB
        hx3918_wear_ppg_Int_handle();
#endif
        break;

    case LAB_TEST_MODE:
#ifdef LAB_TEST
        hx3918_lab_test_Int_handle();
#endif

    default:
        break;
    }

}

#ifdef HRS_ALG_LIB
void hx3918_hrs_ppg_Int_handle(void)
{
    uint8_t        ii = 0;
    hrs_results_t alg_results = {MSG_HRS_ALG_NOT_OPEN, 0, 0, 0, 0};
    hx3918_hrs_wear_msg_code_t hrs_wear_status = MSG_HRS_INIT;
    int32_t *PPG_buf = &(ppg_s_dat.green_data[0]);
    int32_t *ir_buf = &(ppg_s_dat.ir_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
    int16_t gsen_fifo_x_send[32] = {0};
    int16_t gsen_fifo_y_send[32] = {0};
    int16_t gsen_fifo_z_send[32] = {0};
    //HRS_CAL_SET_T cal= get_hrs_agc_status();
#ifdef BP_CUSTDOWN_ALG_LIB
    bp_results_t    bp_alg_results ;
#endif
#ifdef HR_VECTOR
    for (ii = 0; ii < 8; ii++) {
        PPG_buf_vec[ii] = hrm_input_data[hrs_send_cnt + ii];
        gsen_fifo_x[ii] = gsen_input_data_x[hrs_send_cnt + ii];
        gsen_fifo_y[ii] = gsen_input_data_y[hrs_send_cnt + ii];
        gsen_fifo_z[ii] = gsen_input_data_z[hrs_send_cnt + ii];
    }
    hrs_send_cnt = hrs_send_cnt + 8;
    *count = 8;
    tyhx_hrs_alg_send_data(PPG_buf_vec, *count, 0, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z);
#else
    if (hx3918_hrs_read(&ppg_s_dat) == 0) {
        return;
    }

    gsensor_read_cbuf();
    for (ii = 0; ii < *count; ii++) {
#ifdef GSENSER_DATA
        gsen_fifo_x_send[ii] = gsen_fifo_x[32 - *count + ii];
        gsen_fifo_y_send[ii] = gsen_fifo_y[32 - *count + ii];
        gsen_fifo_z_send[ii] = gsen_fifo_z[32 - *count + ii];
#endif
        DEBUG_PRINTF("HX_data:%d/%d  %d %d %d %d %d %d\r\n", ii, *count, PPG_buf[ii], ir_buf[ii], gsen_fifo_x[ii], gsen_fifo_y[ii], gsen_fifo_z[ii], ppg_s_dat.green_cur);
    }
    hrs_wear_status = hx3918_hrs_get_wear_status();
    hrsresult.wearstatus = hrs_wear_status;
    if (hrs_wear_status == MSG_HRS_WEAR) {
        tyhx_hrs_alg_send_data(PPG_buf, *count, gsen_fifo_x_send, gsen_fifo_y_send, gsen_fifo_z_send);
    }
#endif
    alg_results = tyhx_hrs_alg_get_results();
    hrsresult.lastesthrs = alg_results.hr_result;
    DEBUG_PRINTF("hrsresult.wearstatus =%d HR=%d\r\n", hrs_wear_status, alg_results.hr_result);
#ifdef BP_CUSTDOWN_ALG_LIB
    bp_alg_results = tyhx_alg_get_bp_results();
    TYHX_LOG("bp up_value: %d, down_value: %d", bp_alg_results.sbp, bp_alg_results.dbp);
#endif

//            TYHX_LOG("living=%d",hrsresult.hr_living);
#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;
        for (ii = 0; ii < *count; ii++) {
            rawdata.vector_flag = HRS_VECTOR_FLAG;
            rawdata.data_cnt = alg_results.data_cnt - *count + ii;
            rawdata.hr_result = alg_results.hr_result;
            rawdata.red_raw_data = PPG_buf[ii];
            rawdata.ir_raw_data = ir_buf[ii];
            rawdata.gsensor_x = gsen_fifo_x[ii];
            rawdata.gsensor_y = gsen_fifo_y[ii];
            rawdata.gsensor_z = gsen_fifo_z[ii];
            rawdata.red_cur = ppg_s_dat.green_cur;
            rawdata.ir_cur = alg_results.hrs_alg_status;

            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif
}

#ifdef CHECK_LIVING_LIB
void hx3918_living_Int_handle(void)
{

    uint8_t        ii = 0;
    hx3918_living_results_t living_alg_results = {MSG_LIVING_NO_WEAR, 0, 0, 0};

    int32_t *PPG_buf = &(ppg_s_dat.green_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
    hx3918_hrs_wear_msg_code_t     hrs_wear_status = MSG_HRS_NO_WEAR;
    int16_t 	gsen_fifo_x_send[32] = {0};
    int16_t 	gsen_fifo_y_send[32] = {0};
    int16_t 	gsen_fifo_z_send[32] = {0};

    if (hx3918_hrs_read(&ppg_s_dat) == 0) {
        return;
    }

    gsensor_read_cbuf();
    for (ii = 0; ii < *count; ii++) {
        gsen_fifo_x_send[ii] = gsen_fifo_x[32 - *count + ii];
        gsen_fifo_y_send[ii] = gsen_fifo_y[32 - *count + ii];
        gsen_fifo_z_send[ii] = gsen_fifo_z[32 - *count + ii];
        //DEBUG_PRINTF("HX_data:%d/%d %d %d %d %d %d %d\r\n",1+ii,*count,PPG_buf[ii],ir_buf[ii],gsen_fifo_x[ii],gsen_fisignal_qualityfo_y[ii],gsen_fifo_z[ii],hrs_s_dat.agc_green);
    }

    living_alg_results = hx3918_living_get_results();
    //TYHX_LOG("%d %d %d %d\r\n",living_alg_results.data_cnt,living_alg_results.motion_status,living_alg_results.signal_quality,living_alg_results.wear_status);

}
#endif


#ifdef SPO2_ALG_LIB
void hx3918_spo2_ppg_Int_handle(void)
{
    uint8_t        ii = 0;
    tyhx_spo2_results_t alg_results = {MSG_SPO2_ALG_NOT_OPEN, 0, 0, 0, 0, 0, 0};
    SPO2_CAL_SET_T cal = get_spo2_agc_status();
    hx3918_spo2_wear_msg_code_t spo2_wear_status = MSG_SPO2_INIT;
    int32_t *green_buf = &(ppg_s_dat.green_data[0]);
    int32_t *red_buf = &(ppg_s_dat.red_data[0]);
    int32_t *ir_buf = &(ppg_s_dat.ir_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
#ifdef SPO2_DATA_CALI
    int32_t red_data_cali, ir_data_cali;
#endif

#ifdef SPO2_VECTOR
    for (ii = 0; ii < 8; ii++) {
        red_buf_vec[ii] = vec_red_data[spo2_send_cnt + ii];
        ir_buf_vec[ii] = vec_ir_data[spo2_send_cnt + ii];
        green_buf_vec[ii] = vec_green_data[spo2_send_cnt + ii];
    }
    spo2_send_cnt = spo2_send_cnt + 8;
    *count = 8;
    for (ii = 0; ii < 10; ii++) {
        gsen_fifo_x[ii] = vec_red_data[spo2_send_cnt1 + ii];;
        gsen_fifo_y[ii] = vec_ir_data[spo2_send_cnt1 + ii];;
        gsen_fifo_z[ii] = vec_green_data[spo2_send_cnt1 + ii];;
    }
    spo2_send_cnt1 = spo2_send_cnt1 + 10;
    hx3918_spo2_alg_send_data(red_buf_vec, ir_buf_vec, green_buf_vec, *count, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z);
#else
    if (hx3918_spo2_read(&ppg_s_dat) == 0) {
        return;
    }

    gsensor_read_cbuf();
    for (ii = 0; ii < *count; ii++) {
        DEBUG_PRINTF("HX_data: %d/%d %d %d %d %d %d %d %d %d %d\r\n", 1 + ii, *count, \
                     red_buf[ii], ir_buf[ii], cal.red_idac, cal.ir_idac, cal.red_cur, cal.ir_cur, gsen_fifo_x[ii], gsen_fifo_y[ii], gsen_fifo_z[ii]);
    }
    spo2_wear_status = hx3918_spo2_get_wear_status();
    hrsresult.wearstatus = spo2_wear_status;
    if (spo2_wear_status == MSG_SPO2_WEAR) {
        tyhx_spo2_alg_send_data(red_buf, ir_buf, cal.red_idac, cal.ir_idac, *count, gsen_fifo_x, gsen_fifo_y, gsen_fifo_z);
    }
#endif

    alg_results = tyhx_spo2_alg_get_results();
    hrsresult.lastestspo2 = alg_results.spo2_result;
    DEBUG_PRINTF("hrsresult.wearstatus =%d,SPO2=%d\r\n", spo2_wear_status, hrsresult.lastestspo2);

#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;
        for (ii = 0; ii < *count; ii++) {
#ifdef SPO2_DATA_CALI
            ir_data_cali = hx3918_ir_data_cali(ir_buf[ii]);
            red_data_cali = hx3918_red_data_cali(red_buf[ii]);
            rawdata.red_raw_data = red_data_cali;
            rawdata.ir_raw_data = ir_data_cali;
#else
            rawdata.red_raw_data = red_buf[ii];
            rawdata.ir_raw_data = ir_buf[ii];
#endif
            rawdata.vector_flag = SPO2_VECTOR_FLAG;
            //rawdata.data_cnt = alg_results.data_cnt-*count+ii;
            rawdata.data_cnt = cal.green_idac;
            rawdata.hr_result = alg_results.spo2_result;
//            rawdata.gsensor_x = gsen_fifo_x[ii];
//            rawdata.gsensor_y = gsen_fifo_y[ii];
//            rawdata.gsensor_z = gsen_fifo_z[ii];
            rawdata.gsensor_x = green_buf[ii] >> 5;
            rawdata.gsensor_y = cal.red_idac;
            rawdata.gsensor_z = cal.ir_idac;
            rawdata.red_cur = cal.red_cur;
            rawdata.ir_cur = cal.ir_cur;
            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif
}
#endif

#ifdef HRV_ALG_LIB
void hx3918_hrv_ppg_Int_handle(void)
{
    uint8_t        ii = 0;
    hrv_results_t alg_hrv_results = {MSG_HRV_ALG_NOT_OPEN, 0, 0, 0, 0, 0};
    int32_t *PPG_buf = &(ppg_s_dat.green_data[0]);
    int32_t *ir_buf = &(ppg_s_dat.ir_data[0]);
    uint8_t *count = &(ppg_s_dat.count);
    hx3918_hrv_wear_msg_code_t wear_mode;

#ifdef HRV_TESTVEC
    int32_t hrm_raw_data;
    hrm_raw_data = vec_data[vec_data_cnt];
    vec_data_cnt++;
    alg_hrv_results = hx3918_hrv_alg_send_data(hrm_raw_data, 0, 0);
#else
    if (hx3918_hrv_read(&ppg_s_dat) == 0) {
        return;
    }
    for (ii = 0; ii < *count; ii++) {
        //DEBUG_PRINTF("HX_data:%d/%d %d\r\n" ,1+ii,*count,PPG_buf[ii]);
    }
    wear_mode = hx3918_hrv_get_wear_status();
    hrsresult.wearstatus = wear_mode;
    if (wear_mode == MSG_HRV_WEAR) {
//        alg_hrv_results = tyhx_hrv_alg_send_bufdata(PPG_buf, *count, 0);
//        hrsresult.lastesthrv = alg_hrv_results .hrv_result;
//        TYHX_LOG("get the hrv = %d\r\n",alg_hrv_results .hrv_result);
    }
#endif

#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;

        HRS_CAL_SET_T cal = get_hrs_agc_status();
        for (ii = 0; ii < *count; ii++) {
            rawdata.vector_flag = HRS_VECTOR_FLAG;
            rawdata.data_cnt = 0;
            rawdata.hr_result = alg_hrv_results.hrv_result;
            rawdata.red_raw_data = PPG_buf[ii];
            rawdata.ir_raw_data = 0;
            rawdata.gsensor_x = gsen_fifo_x[ii];
            rawdata.gsensor_y = gsen_fifo_y[ii];
            rawdata.gsensor_z = gsen_fifo_z[ii];
            rawdata.red_cur = cal.led_cur;
            rawdata.ir_cur = 0;
            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif
}
#endif


#ifdef CHECK_TOUCH_LIB
void hx3918_wear_ppg_Int_handle(void)
{
    uint8_t count = 0;
    uint8_t ii = 0;
    hx3918_wear_msg_code_t hx3918_check_mode_status = MSG_NO_WEAR;
    count = hx3918_check_touch_read(&ppg_s_dat);
    hx3918_check_mode_status = hx3918_check_touch(ppg_s_dat.s_buf, count);
    hrsresult.wearstatus = hx3918_check_mode_status;


#ifdef HRS_BLE_APP
    {
        rawdata_vector_t rawdata;
        //for(ii=0; ii<*count; ii++)
        //app_ctrl_test = true;
        {
            rawdata.vector_flag = HRS_VECTOR_FLAG;
            rawdata.data_cnt = 0;
            rawdata.hr_result = 0;
            rawdata.red_raw_data = 0;
            rawdata.ir_raw_data = glp[0];
            rawdata.gsensor_x = gbl[0];
            rawdata.gsensor_y = gdiff[1];
            rawdata.gsensor_z = nv_base_data[0];
            rawdata.red_cur = 0;
            rawdata.ir_cur = 0;

            ble_rawdata_vector_push(rawdata);
        }
    }
    ble_rawdata_send_handler();
#endif

}
#endif


void hx3918_lab_test_Int_handle(void)
{
#ifdef LAB_TEST
#if defined(TIMMER_MODE)
    uint32_t data_buf[4];
    hx3918_lab_test_read_packet(data_buf);
    //DEBUG_PRINTF("%d %d %d %d\r\n", data_buf[0], data_buf[1], data_buf[2], data_buf[3]);
#else
    uint8_t count = 0;
    int32_t buf[32];
    count = hx3918_read_fifo_data(buf, 2, 1);
    for (uint8_t i = 0; i < count; i++) {
        //DEBUG_PRINTF("%d/%d %d %d\r\n" ,1+i,count,buf[i*2],buf[i*2+1]);
    }
#endif
#endif
}
#endif //CHIP_HX3918

static u8 sensor_init(void)
{
    if (hx3918_chip_check()) {
        if (gsensor_cbuf) {
            return 1;
        }
        short *data_buf = zalloc(GSENSER_CBUF_SIZE);
        if (data_buf == NULL) {
            log_e("hx3918 malloc gsensor cbuf error1!\n");
            return 0;
        }
        gsensor_cbuf = zalloc(sizeof(cbuffer_t));
        if (gsensor_cbuf == NULL) {
            free(data_buf);
            log_e("hx3918 malloc gsensor cbuf error2!\n");
            return 0;
        }
        cbuf_init(gsensor_cbuf, data_buf, GSENSER_CBUF_SIZE);
        return 1;
    }

    return 0;
}


static int sensor_ctl(u8 cmd, void *arg)
{
    s32 ret = 0;

    // LOG("cmd=%d",cmd);

    switch (cmd) {
    case HR_SENSOR_ENABLE:
        hx3918_init(HRS_MODE);
        break;
    case SPO2_SENSOR_ENABLE:
        hx3918_init(SPO2_MODE);
        break;
    case HR_SENSOR_DISABLE:
        hx3918_hrs_disable();
        break;
    case SPO2_SENSOR_DISABLE:
        hx3918_spo2_disable();
        break;
    case HR_SENSOR_LP_DETECTION:
        hx3918_init(WEAR_MODE);
        break;
    case HR_SENSOR_FACTORY_TEST:
        hx3918_init(FT_LEAK_LIGHT_MODE);
        break;

    case HR_SEARCH_SENSOR: {
        int valid = hx3918_chip_check();
        memcpy(arg, &valid, 4);
        break;
    }
    case INPUT_ACCELEROMETER_DATA: {
        short *data = arg;
        gsensor_write_cbuf(data[0], &data[1]);
        break;
    }
    default:
        break;
    }
    return ret;
}

REGISTER_HR_SENSOR(hrSensor) = {
    .logo = "hx3918",
    .heart_rate_sensor_init  = sensor_init,
    .heart_rate_sensor_check = NULL,
    .heart_rate_sensor_ctl   = sensor_ctl,
} ;
#endif
