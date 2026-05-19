#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".lpctmu_hw.data.bss")
#pragma data_seg(".lpctmu_hw.data")
#pragma const_seg(".lpctmu_hw.text.const")
#pragma code_seg(".lpctmu_hw.text")
#endif
#include "system/includes.h"
#include "asm/power_interface.h"
#include "asm/lpctmu_hw.h"
#include "asm/charge.h"
#include "gpadc.h"

#if 1

#define LOG_TAG_CONST       LP_KEY
#define LOG_TAG             "[LP_KEY]"
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"
#else

#define log_debug   printf

#endif


#define ABS(a, b)         (a > b) ? (a-b) : (b-a)


#define CH_RES_BUF_SIZE     240

static const u8 ch_port[LPCTMU_CHANNEL_SIZE] = {
    IO_PORTB_00,
    IO_PORTB_01,
    IO_PORTB_02,
    IO_PORTB_03,
    IO_PORTB_04,
};

static u32 lpctmu_dma_nvram_size;
struct lpctmu_dma_kfifo res_kfifo;
struct lpctmu_dma_kfifo *dma_kfifo = NULL;
static struct lpctmu_config_data *__this = NULL;
static u32 lpctmu_scan_prd;
static u16 lpctmu_res_oft_min[LPCTMU_CHANNEL_SIZE];
static u8 lpctmu_ch_order_list[LPCTMU_CHANNEL_SIZE];

volatile u8 lpctmu_event = 0;
volatile u8 lpctmu_isr_sleep = 0;
volatile s16 lpctmu_ear_last_diff[2];


extern u32 __get_lrc_hz();


u32 lpctmu_get_cur_ch_by_idx(u32 ch_idx)
{
    ch_idx %= __this->ch_num;
    return __this->ch_list[ch_idx];
}

u32 lpctmu_get_idx_by_cur_ch(u32 cur_ch)
{
    for (u32 ch_idx = 0; ch_idx < __this->ch_num; ch_idx ++) {
        if (cur_ch == __this->ch_list[ch_idx]) {
            return ch_idx;
        }
    }
    return 0;
}

void lpctmu_port_init(u32 ch)
{
    u32 port = ch_port[ch];
    gpio_set_mode(port / 16, BIT(port % 16), PORT_HIGHZ);
}

AT_VOLATILE_RAM_CODE_POWER
void lpctmu_set_io_state(u32 ch, u32 state)
{
    if (ch >= LPCTMU_CHANNEL_SIZE) {
        return;
    }
    JL_PORTB->DIE |=  BIT(ch);
    JL_PORTB->PU  &= ~BIT(ch * 2);
    JL_PORTB->PU  &= ~BIT(ch * 2 + 1);
    JL_PORTB->PD  &= ~BIT(ch * 2);
    JL_PORTB->PD  &= ~BIT(ch * 2 + 1);
    switch (state) {
    case PORT_OUTPUT_LOW:
        JL_PORTB->OUT &= ~BIT(ch);
        JL_PORTB->DIR &= ~BIT(ch);
        break;
    case PORT_OUTPUT_HIGH:
        JL_PORTB->OUT |=  BIT(ch);
        JL_PORTB->DIR &= ~BIT(ch);
        break;
    case PORT_INPUT_PULLUP_10K:
        JL_PORTB->PU  |=  BIT(ch * 2);
        JL_PORTB->DIR |=  BIT(ch);
        break;
    case PORT_INPUT_PULLUP_100K:
        JL_PORTB->PU  |=  BIT(ch * 2 + 1);
        JL_PORTB->DIR |=  BIT(ch);
        break;
    case PORT_INPUT_PULLDOWN_10K:
        JL_PORTB->PD  |=  BIT(ch * 2);
        JL_PORTB->DIR |=  BIT(ch);
        break;
    case PORT_INPUT_PULLDOWN_100K:
        JL_PORTB->PD  |=  BIT(ch * 2 + 1);
        JL_PORTB->DIR |=  BIT(ch);
        break;
    default:
        JL_PORTB->DIE &= ~BIT(ch);
        JL_PORTB->DIR |=  BIT(ch);
        break;
    }
}

void lpctmu_set_ana_hv_level(u32 level)
{
    SFR(P11_LPCTM0->ANA0, 5, 3, level & 0b111);
}

u32 lpctmu_get_ana_hv_level(void)
{
    u32 level = (P11_LPCTM0->ANA0 >> 5) & 0b111;
    return level;
}

void lpctmu_vsel_trim(void)
{
    if (__this->pdata->aim_vol_delta < 8) {
        lpctmu_set_ana_hv_level(__this->pdata->aim_vol_delta);
        SFR(P11_LPCTM0->ANA1, 0, 2, 0b00);//关闭HV/LV
        log_debug("hv_level = %d", __this->pdata->aim_vol_delta);
        return;
    }
    SFR(P11_LPCTM0->ANA1, 0, 2, 0b01);//先使能LV
    u32 lv_vol = adc_get_voltage_blocking(AD_CH_LPCTMU);  //阻塞式采集一个指定通道的电压值（经过均值滤波处理）
    log_debug("lv_vol = %dmv", lv_vol);

    SFR(P11_LPCTM0->ANA1, 0, 2, 0b10);//再使能HV
    u32 delta, diff, diff_min = -1;
    u8 aim_hv_level = 7;
    for (u8 hv_level = 0; hv_level < 8; hv_level ++) {
        lpctmu_set_ana_hv_level(hv_level);
        u32 hv_vol = adc_get_voltage_blocking(AD_CH_LPCTMU);  //阻塞式采集一个指定通道的电压值（经过均值滤波处理）
        log_debug("hv_vol = %dmv", hv_vol);
        if (hv_vol > lv_vol) {
            delta = hv_vol - lv_vol;
        } else {
            delta = lv_vol - hv_vol;
        }
        if (delta > __this->pdata->aim_vol_delta) {
            diff = delta - __this->pdata->aim_vol_delta;
        } else {
            diff = __this->pdata->aim_vol_delta - delta;
        }
        if (diff_min >= diff) {
            diff_min = diff;
            aim_hv_level = hv_level;
        }
    }
    log_debug("hv_level = %d  diff %d", aim_hv_level, diff_min);
    lpctmu_set_ana_hv_level(aim_hv_level);
    SFR(P11_LPCTM0->ANA1, 0, 2, 0b00);//关闭HV/LV
}


u32 lpctmu_get_charge_clk(void)
{
    JL_GPCNT0->CON = BIT(30);

    SFR(JL_GPCNT0->CON,  1,  5,  17);//次时钟选择p11_dbg_out

    SFR(JL_GPCNT0->CON, 16,  4,  8); //主时钟周期数：32 * 2^n = 8192
    SFR(JL_GPCNT0->CON,  8,  5,  6); //主时钟选择std24m

    JL_GPCNT0->CON |= BIT(30) | BIT(0);

    while (!(JL_GPCNT0->CON & BIT(31)));
    u32 gpcnt_num = JL_GPCNT0->NUM;
    JL_GPCNT0->CON &= ~BIT(0);

    u32 std24m_clk_khz = 24000;
    u32 charge_clk = std24m_clk_khz * gpcnt_num / 8192;
    log_debug("gpcnt_num = %d charge_clk = %dKHz\n", gpcnt_num, charge_clk);
    return charge_clk;
}

void lpctmu_set_ana_cur_level(u32 ch, u32 cur_level)
{
    SFR(P11_LPCTM0->CHIS, ch * 4, 3, cur_level);
}

u32 lpctmu_get_ana_cur_level(u32 ch)
{
    u32 level = (P11_LPCTM0->CHIS >> (ch * 4)) & 0b111;
    return level;
}

void lpctmu_cur_trim_by_res(u32 ch, u32 ch_res)
{
    if (!__this) {
        return;
    }
    if (__this->pdata->aim_charge_khz < 8) {
        return;
    }
    if ((!ch_res) || (lpctmu_res_oft_min[ch] == 0)) {
        lpctmu_res_oft_min[ch] = 0xfff0;
        return;
    }
    if (lpctmu_res_oft_min[ch] > 0xfff0) {
        return;
    }
    u32 net_level, tmp_min;
    u32 cur_level = lpctmu_get_ana_cur_level(ch);
    u32 trim_aim_res = __this->pdata->aim_charge_khz * __this->pdata->sample_window_time;
    if (ch_res < trim_aim_res) {
        if (cur_level >= 7) {
            lpctmu_res_oft_min[ch] = -1;
            return;
        }
        net_level = cur_level + 1;
        tmp_min = trim_aim_res - ch_res;
    } else {
        if (cur_level == 0) {
            lpctmu_res_oft_min[ch] = -1;
            return;
        }
        net_level = cur_level - 1;
        tmp_min = ch_res - trim_aim_res;
    }
    if (tmp_min == 0) {
        lpctmu_res_oft_min[ch] = -1;
        return;
    }
    if (tmp_min < lpctmu_res_oft_min[ch]) {
        lpctmu_res_oft_min[ch] = tmp_min;
    } else {
        lpctmu_res_oft_min[ch] = -1;
    }
    lpctmu_set_ana_cur_level(ch, net_level);
    log_debug("ch:%d res:%d cur:%d net:%d\n", ch, ch_res, cur_level, net_level);
}

void lpctmu_isel_trim(u32 ch)
{
    SFR(P11_LPCTM0->ANA1, 4, 4, ch + 1);//使能对应通道

    u32 diff, diff_min = -1;
    u8 aim_cur_level;
    if (__this->pdata->aim_charge_khz < 8) {
        aim_cur_level = __this->pdata->aim_charge_khz;
    } else if (__this->ch_fixed_isel[ch]) {
        aim_cur_level = __this->ch_fixed_isel[ch];
    } else {
        for (u8 cur_level = 0; cur_level < 8; cur_level ++) {
            SFR(P11_LPCTM0->ANA0, 1, 3, cur_level);
            lpctmu_set_ana_cur_level(ch, cur_level);
            u32 charge_clk = lpctmu_get_charge_clk();
            if (charge_clk > __this->pdata->aim_charge_khz) {
                diff = charge_clk - __this->pdata->aim_charge_khz;
            } else {
                diff = __this->pdata->aim_charge_khz - charge_clk;
            }
            if (diff_min >= diff) {
                diff_min = diff;
                aim_cur_level = cur_level;
            }
        }
    }
    log_debug("ch%d cur_level = %d  diff %d", ch, aim_cur_level, diff_min);
    SFR(P11_LPCTM0->ANA0, 1, 3, aim_cur_level);
    lpctmu_set_ana_cur_level(ch, aim_cur_level);
}

void lpctmu_vsel_isel_trim(void)
{
    SFR(P11_LPCTM0->ANA1, 3, 1, 1);//软件控制
    SFR(P11_LPCTM0->ANA1, 2, 1, 1);//模拟模块偏置使能
    SFR(P11_LPCTM0->ANA0, 0, 1, 1);//模拟模块使能
    SFR(P11_LPCTM0->ANA1, 4, 4, 0);//先不使能任何通道

    lpctmu_vsel_trim();

    for (u32 ch_idx = 0; ch_idx < __this->ch_num; ch_idx ++) {
        lpctmu_isel_trim(__this->ch_list[ch_idx]);
    }
}

AT_VOLATILE_RAM_CODE_POWER
u32 lpctmu_addr_to_ch_order(u32 addr)
{
    u32 idx_max = __this->ch_num;
    if (__this->ch_ear_en) {
        if (__this->ear_sample_dis) {
            idx_max = __this->ch_key_num;
        } else {
            idx_max = __this->ch_key_num + 2;
        }
    }

    u32 offset = 1;
    u32 idx = idx_max - 1;

__remainder:

    if (((addr + offset) % idx_max) == 0) {
        return idx;
    }
    offset ++;
    idx --;
    if (idx) {
        goto __remainder;
    }
    return 0;
}

u32 lpctmu_get_ch_idx_by_res_kfifo_out(void)
{
    if (!__this) {
        return 0;
    }
    u32 order = lpctmu_addr_to_ch_order(res_kfifo.buf_out);
    u32 cur_ch = lpctmu_ch_order_list[order];
    return lpctmu_get_idx_by_cur_ch(cur_ch);
}

u32 lpctmu_check_dma_kfifo_loop_mark(void)
{
    u32 out = 0;
    u32 offset = __this->ch_num * 2;
    if (dma_kfifo->buf_out > offset) {
        out = dma_kfifo->buf_out - offset - 1;
    } else {
        out = dma_kfifo->buf_out + dma_kfifo->buf_size - offset - 1;
        out %= dma_kfifo->buf_size;
    }
    if (dma_kfifo->buffer[out] == 0xffff) {
        return 1;
    }
    return 0;
}

void lpctmu_dma_kfifo_loop_mark(void)
{
    u32 out = 0;
    u32 offset = __this->ch_num * 2;
    if (dma_kfifo->buf_out > offset) {
        out = dma_kfifo->buf_out - offset - 1;
    } else {
        out = dma_kfifo->buf_out + dma_kfifo->buf_size - offset - 1;
        out %= dma_kfifo->buf_size;
    }
    dma_kfifo->buffer[out] = 0xffff;
}

AT_VOLATILE_RAM_CODE_POWER
void lpctmu_save_one_res_to_kfifo(u32 buf_out, u32 ch_res)
{
    u32 buf_in = res_kfifo.buf_in % res_kfifo.buf_size;
    u32 out_order = lpctmu_addr_to_ch_order(buf_out);
    u32 in_order  = lpctmu_addr_to_ch_order(buf_in);
    if (out_order != in_order) {
        /* log_debug("in:%d %d out:%d %d\n", res_kfifo.buf_in, in_order, buf_out, out_order); */
        return;
    }
    res_kfifo.buffer[buf_in] = ch_res;
    res_kfifo.buf_in ++;
}

void lpctmu_move_dma_data_to_res_kfifo(void)
{
    __asm__ volatile("csync");
    u32 dma_base_offset = __this->ch_num * 4;
    dma_kfifo->buf_in = (P11_LPCTM0->DMA_WADR / 2) - (dma_base_offset / 2);

    u32 len;
    u32 mark = lpctmu_check_dma_kfifo_loop_mark();
    if (mark) {
        len = dma_kfifo->buf_in + dma_kfifo->buf_size - dma_kfifo->buf_out;
        if (dma_kfifo->buf_in >= dma_kfifo->buf_out) {
            len = dma_kfifo->buf_in - dma_kfifo->buf_out;
        }
    } else {
        len = dma_kfifo->buf_size;
        dma_kfifo->buf_out = dma_kfifo->buf_in;
    }
    /* log_debug("mv len:%d", len); */

    u32 out = dma_kfifo->buf_out;
    for (u32 i = 0; i < len; i ++) {
        lpctmu_save_one_res_to_kfifo(out, dma_kfifo->buffer[out]);
        out ++;
        out %= dma_kfifo->buf_size;
    }
    dma_kfifo->buf_out = dma_kfifo->buf_in;
    lpctmu_dma_kfifo_loop_mark();
}

AT_VOLATILE_RAM_CODE_POWER
u32 lpctmu_get_res_kfifo_length(void)
{
    if (!__this) {
        return 0;
    }
    u32 buf_len = res_kfifo.buf_in - res_kfifo.buf_out;
    if (buf_len > res_kfifo.buf_size) {
        buf_len = res_kfifo.buf_size;
        res_kfifo.buf_out = res_kfifo.buf_in - buf_len;
    }
    /* log_debug("do len:%d", buf_len); */
    return buf_len;
}

void lpctmu_abandon_dma_data(void)
{
    if (!__this) {
        return;
    }
    if (__this->ch_ear_en == 0) {
        __asm__ volatile("csync");
        u32 dma_base_offset = __this->ch_num * 4;
        dma_kfifo->buf_in = (P11_LPCTM0->DMA_WADR / 2) - (dma_base_offset / 2);
        dma_kfifo->buf_out = dma_kfifo->buf_in;
        lpctmu_dma_kfifo_loop_mark();
    }
}

u32 lpctmu_get_res_kfifo_data(u16 *buf, u32 len)
{
    if (!__this) {
        return 0;
    }
    u32 buf_len = lpctmu_get_res_kfifo_length();
    u32 buf_out = res_kfifo.buf_out % res_kfifo.buf_size;
    u32 first_len = res_kfifo.buf_size - buf_out;
    len = MIN(len, buf_len);
    first_len = MIN(len, first_len);
    for (u32 i = 0; i < first_len; i ++) {
        buf[i] = res_kfifo.buffer[buf_out + i];
    }
    for (u32 i = 0; i < (len - first_len); i ++) {
        buf[first_len + i] = res_kfifo.buffer[i];
    }
    res_kfifo.buf_out += len;
    return len;
}

AT_VOLATILE_RAM_CODE_POWER
s16 lpctmu_res_medfilt3(u32 ch, s16 ch_res)
{
    if (__this->ch_pp_res[ch] == 0) {
        __this->ch_pp_res[ch] = ch_res;
        __this->ch_p_res[ch] = ch_res;
    }
    s16 med_res = ch_res;
    if (__this->ch_pp_res[ch] > __this->ch_p_res[ch]) {
        if (ch_res > __this->ch_pp_res[ch]) {
            med_res = __this->ch_pp_res[ch];
        }
        if (ch_res < __this->ch_p_res[ch]) {
            med_res = __this->ch_p_res[ch];
        }
    } else {
        if (ch_res > __this->ch_p_res[ch]) {
            med_res = __this->ch_p_res[ch];
        }
        if (ch_res < __this->ch_pp_res[ch]) {
            med_res = __this->ch_pp_res[ch];
        }
    }

    __this->ch_pp_res[ch] = __this->ch_p_res[ch];
    __this->ch_p_res[ch] = ch_res;

    return med_res;
}

AT_VOLATILE_RAM_CODE_POWER
void lpctmu_eartch_sample(void)
{
    u32 ref_ch;
    u32 next_ch = __this->ear_ch_list[__this->ear_group_cnt][__this->ear_sample_cnt];
    u32 next_ch_en = BIT(next_ch);
    SFR(P11_LPCTM0->CHEN, 0, 5, next_ch_en);

    lpctmu_set_io_state(next_ch, PORT_HIGHZ);

    if (__this->ear_sample_cnt == 0) {
        ref_ch = __this->ear_ch_list[__this->ear_group_cnt][1];
    } else {
        ref_ch = __this->ear_ch_list[__this->ear_group_cnt][0];
    }
    u32 ref_io_mode = __this->ear_ref_io_mode[__this->ear_group_cnt];
    switch (ref_io_mode) {
    case 1:
        lpctmu_set_io_state(ref_ch, PORT_OUTPUT_LOW);
        break;
    case 2:
        lpctmu_set_io_state(ref_ch, PORT_OUTPUT_HIGH);
        break;
    default :
        lpctmu_set_io_state(ref_ch, PORT_HIGHZ);
        break;
    }
    __this->ear_sample_cnt ++;
    if (__this->ear_sample_cnt >= __this->ear_sample_cnt_max) {
        __this->ear_sample_cnt  = 0;
        __this->ear_group_cnt ++;
        if (__this->ear_group_cnt >= __this->ear_group_cnt_max) {
            __this->ear_group_cnt  = 0;
        }
    }

    SFR(P11_LPCTM0->WCON, 0, 1, 1);     //kick
}

AT_VOLATILE_RAM_CODE_POWER
void lpctmu_isr_deal(void)
{
    u32 cur_ch, ch_res, res;
    u32 pnd_type = 0;
    if ((__this->ch_ear_en) && (P11_LPCTM0->CON0 & BIT(7)) && (P11_LPCTM0->CON0 & BIT(1))) { //RES PND
        cur_ch = (P11_LPCTM0->RES >> 29) & 0b111;
        ch_res = P11_LPCTM0->RES & 0x1fff;//24位取13位
        pnd_type |= LPCTMU_HW_RES_PND;
        if (lpctmu_event & BIT(cur_ch)) {
            P11_LPCTM0->WCON |= BIT(6);//clr res pnd
            __asm__ volatile("csync");
            goto __next_sample;
        }
        res = ch_res | (cur_ch << 13);
        lpctmu_save_one_res_to_kfifo(__this->sample_cnt, res);
        __this->sample_cnt ++;
        if (lpctmu_isr_sleep == 0) {
            u32 len = lpctmu_get_res_kfifo_length();
            if (len > (res_kfifo.buf_size / 2)) {
                lpctmu_event |= BIT(cur_ch);
            }
        }
        s16 med_res = lpctmu_res_medfilt3(cur_ch, ch_res);
        if (__this->sample_cnt <= __this->ch_key_num) {
            if ((med_res < (s16)__this->lim_l[cur_ch]) || (med_res > (s16)__this->lim_h[cur_ch])) {
                lpctmu_event |= BIT(cur_ch);
            }
        } else {
            if (__this->sample_cnt >= __this->sample_cnt_max) {
                u32 group = 1;
                if (cur_ch == __this->ear_ch_list[0][1]) {
                    group = 0;
                }
                u32 tmp_ch = __this->ear_ch_list[group][0];
                u32 tmp_ref_ch = __this->ear_ch_list[group][1];
                s16 diff = (s16)__this->ch_p_res[tmp_ch] - (s16)__this->ch_p_res[tmp_ref_ch];
                diff = lpctmu_res_medfilt3(LPCTMU_CHANNEL_SIZE + group, diff);
                s16 delta = ABS(diff, lpctmu_ear_last_diff[group]);
                if (delta > 10) {
                    lpctmu_event |= BIT(cur_ch);
                }
                if (lpctmu_event) {
                    lpctmu_ear_last_diff[group] = diff;
                }
            }
        }
        if (lpctmu_event) {
            if (lpctmu_isr_sleep) {
                return;
            } else {
                P11_LPCTM0->WCON |= BIT(6);//clr res pnd
                __asm__ volatile("csync");
            }
        } else {
            P11_LPCTM0->WCON |= BIT(6);//clr res pnd
            __asm__ volatile("csync");
        }
    } else if (__this->ch_ear_en == 0) {
        if ((P11_LPCTM0->CON0 & BIT(7)) && (P11_LPCTM0->CON0 & BIT(1))) { //RES PND
            P11_LPCTM0->WCON |= BIT(6);//clr res pnd
            __asm__ volatile("csync");
            pnd_type |= LPCTMU_HW_RES_PND;
        }
        if ((P11_LPCTM0->DMA_CON & BIT(7)) && (P11_LPCTM0->DMA_CON & BIT(2))) {//DMA FULL PND
            P11_LPCTM0->WCON |= BIT(5);//clr dma full pnd
            __asm__ volatile("csync");
            pnd_type |= LPCTMU_DMA_FULL_PND;
        }
        if ((P11_LPCTM0->DMA_CON & BIT(6)) && (P11_LPCTM0->DMA_CON & BIT(3))) {//DMA HALF PND
            P11_LPCTM0->WCON |= BIT(4);//clr dma half pnd
            __asm__ volatile("csync");
            pnd_type |= LPCTMU_DMA_HALF_PND;
        }
        if ((P11_LPCTM0->DMA_CON & BIT(5)) && (P11_LPCTM0->DMA_CON & BIT(1))) {//DMA PND
            P11_LPCTM0->WCON |= BIT(3);//clr dma pnd
            __asm__ volatile("csync");
            pnd_type |= LPCTMU_DMA_RES_PND;
        }
        if ((P11_LPCTM0->MSG_CON & BIT(8)) && (P11_LPCTM0->MSG_CON & BIT(2))) {//LIM PND
            P11_LPCTM0->WCON |= BIT(2);//clr key pnd
            __asm__ volatile("csync");
            pnd_type |= LPCTMU_KEY_MSG_PND;
        }
        /* log_debug("pnd_type:0x%x", pnd_type); */
        /* log_debug("wadr:%d", P11_LPCTM0->DMA_WADR); */
        lpctmu_move_dma_data_to_res_kfifo();
        if (__this->isr_cbfunc) {
            __this->isr_cbfunc(pnd_type);
        }
        return;
    }

__next_sample:

    if (__this->sample_cnt < __this->ch_key_num) {
        goto __post_event;
    }

    if ((__this->ear_sample_dis == 0) && (__this->sample_cnt < __this->sample_cnt_max)) {
        lpctmu_eartch_sample();
    }

__post_event:

    if (!(__this->isr_cbfunc)) {
        return;
    }
    if (lpctmu_isr_sleep) {
        return;
    }
    if (lpctmu_event == 0) {
        return;
    }
    if ((__this->ear_sample_dis) && (__this->sample_cnt >= __this->ch_key_num)) {
        __this->isr_cbfunc(pnd_type);
    } else if (__this->sample_cnt >= __this->sample_cnt_max) {
        __this->isr_cbfunc(pnd_type);
    }
}

___interrupt
void lpctmu_isr(void)
{
    lpctmu_isr_deal();
}

AT_VOLATILE_RAM_CODE_POWER
void lpctmu_lptimer_isr_deal(void)
{
    P3_LP_TMR1_CON |= BIT(6);
    P3_LP_TMR1_CON |= BIT(4);

    if (!__this) {
        return;
    }

    __this->sample_cnt = 0;
    __this->ear_sample_cnt = 0;
    lpctmu_event &= ~(BIT(LPCTMU_CHANNEL_SIZE) - 1);

    if (__this->ch_key_en) {
        SFR(P11_LPCTM0->CHEN, 0, 5, __this->ch_key_en);
        SFR(P11_LPCTM0->WCON, 0, 1, 1);     //kick
        return;
    }
    lpctmu_isr_deal();
}

___interrupt
void lpctmu_lptimer_isr(void)
{
    lpctmu_lptimer_isr_deal();
}

AT_VOLATILE_RAM_CODE_POWER
u32 is_light_sleep_continue()
{
    if (!__this) {
        return 0;
    }
    if (__this->ch_ear_en == 0) {
        return 0;
    }
    lpctmu_isr_sleep = 1;
    if (P3_LP_TMR1_CON & BIT(5)) {
        lpctmu_lptimer_isr_deal();
        lpctmu_isr_sleep = 0;
        return 1;
    }
    if (P11_LPCTM0->CON0 & BIT(7)) { //RES PND
        lpctmu_isr_deal();
        if (lpctmu_event == 0) {
            lpctmu_isr_sleep = 0;
            return 1;
        }
    }
    lpctmu_isr_sleep = 0;
    return 0;
}

void lpctmu_eartch_enable(void)
{
    if (!__this) {
        return;
    }
    __this->ear_sample_dis = 0;
}

void lpctmu_eartch_disable(void)
{
    if (!__this) {
        return;
    }
    __this->ear_sample_dis = 1;
}

u32 lpctmu_get_lptimer_prd(void)
{
    u32 prd = (P3_LP_PRD10 << 24) | \
              (P3_LP_PRD11 << 16) | \
              (P3_LP_PRD12 <<  8) | \
              (P3_LP_PRD13 <<  0);
    return prd;
}

void lpctmu_set_lptimer_prd(u32 prd)
{
    SFR(P3_LP_TMR1_CFG, 1, 1, 0);
    P3_LP_PRD10 = (prd >> 24) & 0xff;
    P3_LP_PRD11 = (prd >> 16) & 0xff;
    P3_LP_PRD12 = (prd >>  8) & 0xff;
    P3_LP_PRD13 = (prd >>  0) & 0xff;
    SFR(P3_LP_TMR1_CFG, 1, 1, 1);
}

void lpctmu_lptimer_disable(void)
{
    P33_CON_SET(P3_VDET_CON0, 6, 2, 1);//vdet sel lptimer1

    u32 prd_old = lpctmu_get_lptimer_prd();
    if (prd_old > lpctmu_scan_prd) {
        lpctmu_set_lptimer_prd(prd_old - 1);
        return;
    }

    P3_LP_TMR1_CON &= ~BIT(0);
}

void lpctmu_lptimer_init(u32 scan_time, u32 ie)
{
    u32 lrc_clk = __get_lrc_hz();
    u32 prd_50ms = lrc_clk * 50 / 1000;
    u32 prd_old = lpctmu_get_lptimer_prd();

    P33_CON_SET(P3_VDET_CON0, 6, 2, 2);//vdet sel ctmu

    if ((P3_LP_TMR1_CON & BIT(0)) && (prd_old < prd_50ms) && (scan_time < 50)) {//
        lpctmu_scan_prd = prd_old;
        lpctmu_set_lptimer_prd(prd_old + 1);
        return;
    }

    u32 prd = lrc_clk * scan_time / 1000;
    u32 rsc = prd - (lrc_clk * 5 / 10000);//提前500us

    lpctmu_scan_prd = prd;

    SFR(P3_LP_TMR1_CLK, 0, 3, 4);//SEL LRC_CLK
    SFR(P3_LP_TMR1_CLK, 4, 2, 0);//DIV1

    P3_LP_TMR1_CFG = 0;

    P3_LP_TMR1_CON = BIT(6);

    SFR(P3_LP_TMR1_CON, 3, 1, 0);//to ie
    SFR(P3_LP_TMR1_CON, 2, 1, ie);//wkp ie

    SFR(P3_LP_TMR1_CON, 1, 1, 1);//ctu mode

    P3_LP_RSC10 = (rsc >> 24) & 0xff;
    P3_LP_RSC11 = (rsc >> 16) & 0xff;
    P3_LP_RSC12 = (rsc >>  8) & 0xff;
    P3_LP_RSC13 = (rsc >>  0) & 0xff;

    P3_LP_PRD10 = (prd >> 24) & 0xff;
    P3_LP_PRD11 = (prd >> 16) & 0xff;
    P3_LP_PRD12 = (prd >>  8) & 0xff;
    P3_LP_PRD13 = (prd >>  0) & 0xff;

    SFR(P3_LP_TMR1_CFG, 1, 1, 1);
    SFR(P3_LP_TMR1_CFG, 0, 1, ie);

    P3_LP_TMR1_CON |= BIT(6) | BIT(0);//lptimer1 en

    SFR(P3_LP_TMR1_CFG, 2, 1, 1);
    SFR(JL_PMU->PMU_CON, 9, 1, 1);//kick star
    asm("csync");
    SFR(P3_LP_TMR1_CFG, 2, 1, 0);

}

void lpctmu_cache_ch_res_key_msg_lim(u32 ch, u16 lim_l, u16 lim_h)
{
    if (!__this) {
        return;
    }
    if (lim_h >= lim_l) {
        if (__this->lim_span_max[ch] < (lim_h - lim_l)) {
            lim_h = -1;
            lim_l = -1;
        }
    }
    __this->lim_l[ch] = lim_l;
    __this->lim_h[ch] = lim_h;
}

void lpctmu_ch_res_key_msg_lim_upgrade(void)
{
    if (!__this) {
        return;
    }
    u32 ch_idx, ch;
    u16 *lim = (u16 *)(__this->pdata->dma_nvram_addr);
    for (ch_idx = 0; ch_idx < __this->ch_num; ch_idx ++) {
        ch = lpctmu_ch_order_list[ch_idx];
        if (lim[(ch_idx * 2)] == __this->lim_h[ch]) {
        } else {
            lim[(ch_idx * 2)]  = __this->lim_h[ch];
        }
        if (lim[(ch_idx * 2 + 1)] == __this->lim_l[ch]) {
        } else {
            lim[(ch_idx * 2 + 1)]  = __this->lim_l[ch];
        }
    }
}

void lpctmu_dma_init(void)
{
    memset((u8 *)(__this->pdata->dma_nvram_addr), 0xff, lpctmu_dma_nvram_size);

    u32 dma_base_offset = __this->ch_num * 4;
    u32 dma_len = lpctmu_dma_nvram_size / (__this->ch_num * 2) * (__this->ch_num * 2);

    dma_kfifo->buffer = (u16 *)(__this->pdata->dma_nvram_addr + dma_base_offset);
    dma_kfifo->buf_size = (dma_len - dma_base_offset) / 2;
    dma_kfifo->buf_in = 0;
    dma_kfifo->buf_out = 0;

    lpctmu_ch_res_key_msg_lim_upgrade();

    P11_LPCTM0->DMA_START_ADR = __this->pdata->dma_nvram_addr;
    P11_LPCTM0->DMA_HALF_ADR  = __this->pdata->dma_nvram_addr + dma_base_offset + (dma_len - dma_base_offset) / 2 - 2;
    P11_LPCTM0->DMA_END_ADR   = __this->pdata->dma_nvram_addr + dma_len - 2;

    SFR(P11_LPCTM0->DMA_CON, 2, 1, 1);//DMA FULL IE
    SFR(P11_LPCTM0->DMA_CON, 3, 1, 1);//DMA HALF IE
    SFR(P11_LPCTM0->DMA_CON, 1, 1, 0);//DMA IE

    SFR(P11_LPCTM0->DMA_CON, 0, 1, 1);//DMA EN

    SFR(P11_LPCTM0->MSG_CON, 2, 1, 1);//KEY IE
    SFR(P11_LPCTM0->MSG_CON, 1, 1, 1);//三点中值滤波 EN
    SFR(P11_LPCTM0->MSG_CON, 0, 1, 1);//KEY EN
}

void lpctmu_set_dma_res_ie(u32 en)
{
    if (!__this) {
        return;
    }
    if (__this->ch_ear_en) {
        OS_ENTER_CRITICAL();
        if (en) {
            lpctmu_event |=  BIT(LPCTMU_CHANNEL_SIZE);
        } else {
            lpctmu_event &= ~BIT(LPCTMU_CHANNEL_SIZE);
        }
        OS_EXIT_CRITICAL();
        return;
    }
    if ((!!(P11_LPCTM0->DMA_CON & BIT(1))) == en) {
        return;
    }
    if (en) {
        P11_LPCTM0->WCON |= BIT(3);//clr dma pnd
        SFR(P11_LPCTM0->DMA_CON, 1, 1, 1);//DMA IE
    } else {
        SFR(P11_LPCTM0->DMA_CON, 1, 1, 0);//DMA IE
        P11_LPCTM0->WCON |= BIT(3);//clr dma pnd
    }
}

void lpctmu_dump(void)
{
    log_debug("P11_LPCTM0->CON0           = 0x%x", P11_LPCTM0->CON0);
    log_debug("P11_LPCTM0->CHEN           = 0x%x", P11_LPCTM0->CHEN);
    log_debug("P11_LPCTM0->CNUM           = 0x%x", P11_LPCTM0->CNUM);
    log_debug("P11_LPCTM0->PPRD           = 0x%x", P11_LPCTM0->PPRD);
    log_debug("P11_LPCTM0->DPRD           = 0x%x", P11_LPCTM0->DPRD);
    log_debug("P11_LPCTM0->ECON           = 0x%x", P11_LPCTM0->ECON);
    log_debug("P11_LPCTM0->EXEN           = 0x%x", P11_LPCTM0->EXEN);
    log_debug("P11_LPCTM0->CHIS           = 0x%x", P11_LPCTM0->CHIS);
    log_debug("P11_LPCTM0->CLKC           = 0x%x", P11_LPCTM0->CLKC);
    log_debug("P11_LPCTM0->WCON           = 0x%x", P11_LPCTM0->WCON);
    log_debug("P11_LPCTM0->ANA0           = 0x%x", P11_LPCTM0->ANA0);
    log_debug("P11_LPCTM0->ANA1           = 0x%x", P11_LPCTM0->ANA1);
    log_debug("P11_LPCTM0->RES            =   %d", P11_LPCTM0->RES);
    log_debug("P11_LPCTM0->DMA_START_ADR  =   %d", P11_LPCTM0->DMA_START_ADR);
    log_debug("P11_LPCTM0->DMA_HALF_ADR   =   %d", P11_LPCTM0->DMA_HALF_ADR);
    log_debug("P11_LPCTM0->DMA_END_ADR    =   %d", P11_LPCTM0->DMA_END_ADR);
    log_debug("P11_LPCTM0->DMA_CON        = 0x%x", P11_LPCTM0->DMA_CON);
    log_debug("P11_LPCTM0->MSG_CON        = 0x%x", P11_LPCTM0->MSG_CON);
    log_debug("P11_LPCTM0->DMA_WADR       = 0x%x", P11_LPCTM0->DMA_WADR);
    log_debug("P11_LPCTM0->SLEEP_CON      = 0x%x", P11_LPCTM0->SLEEP_CON);
    log_debug("P11_SYSTEM->P2M_INT_IE     = 0x%x", P11_SYSTEM->P2M_INT_IE);
    log_debug("P11_SYSTEM->P11_SYS_CON1   = 0x%x", P11_SYSTEM->P11_SYS_CON1);
}

void lpctmu_init(struct lpctmu_config_data *cfg_data)
{
    __this = cfg_data;
    if (!__this) {
        return;
    }

    for (u32 ch_idx = 0; ch_idx < __this->ch_num; ch_idx ++) {
        lpctmu_port_init(__this->ch_list[ch_idx]);
    }

    for (u32 idx = 0, ch = 0; ch < LPCTMU_CHANNEL_SIZE; ch ++) {
        lpctmu_ch_order_list[ch] = 0;
        if (__this->ch_en & BIT(ch)) {
            lpctmu_ch_order_list[idx] = ch;
            idx ++;
        }
    }

    u16 *ch_res_buf = (u16 *)malloc(CH_RES_BUF_SIZE * 2);
    res_kfifo.buffer = ch_res_buf;
    res_kfifo.buf_size = CH_RES_BUF_SIZE;
    res_kfifo.buf_in = 0;
    res_kfifo.buf_out = 0;

    u32 dma_base_offset = __this->ch_num * 4;
    lpctmu_dma_nvram_size = __this->pdata->dma_nvram_size + dma_base_offset;
    dma_kfifo = (struct lpctmu_dma_kfifo *)(__this->pdata->dma_nvram_addr + lpctmu_dma_nvram_size);

    u32 hw_init = 0;
    if (!is_wakeup_source(PWR_WK_REASON_P11)) {
        hw_init = 1;
    } else if (__this->ch_ear_en == 0) {
        if (__this->ch_num > 1) {
            u32 cur_buf_in = (P11_LPCTM0->DMA_WADR / 2) - (dma_base_offset / 2);
            if (lpctmu_addr_to_ch_order(cur_buf_in)) {
                hw_init = 1;
            }
        }
    }

    if (hw_init) {

        /* lpctmu_lptimer_disable(); */

        memset(P11_LPCTM0, 0, sizeof(P11_LPCTM_TypeDef));

        //时钟源配置
        u32 lpctmu_clk = __get_lrc_hz();
        log_debug("lpctmu_clk = %dHz", lpctmu_clk);

        SFR(P11_LPCTM0->CLKC, 0, 2, 0);  //ctm_clk_p sel lrc200KHz
        //设置分频
        SFR(P11_LPCTM0->CLKC, 6, 1, 0); //divB = 1分频
        SFR(P11_LPCTM0->CLKC, 7, 1, 0); //divC = 1分频
        SFR(P11_LPCTM0->CLKC, 3, 3, 0); //div  = 2^0 = 1分频

        //通道采集前的待稳定时间配置
        SFR(P11_LPCTM0->PPRD, 4, 4, 9);      //prp_prd = (9 + 1) * t 约等 50us > 10us
        //多通道轮询采集, 切通道时, 通道与通道的间隙时间配置
        SFR(P11_LPCTM0->PPRD, 0, 4, 9);      //stop_prd= (9 + 1) * t 约等 50us > 10us

        //每个通道采集的周期，常设几个毫秒
        u32 det_prd = __this->pdata->sample_window_time * lpctmu_clk / 1000 - 1;
        SFR(P11_LPCTM0->DPRD, 0, 12, det_prd);

        if (__this->pdata->ext_stop_ch_en) {
            SFR(P11_LPCTM0->EXEN, 0, 5, __this->pdata->ext_stop_ch_en);
            SFR(P11_LPCTM0->ECON, 2, 2, __this->pdata->ext_stop_sel);
            SFR(P11_LPCTM0->ECON, 0, 1, 1);//与蓝牙的互斥使能
            /* extern void set_bt_wl_rab_wl2ext(u8 wl2ext_act0, u8 wl2ext_act1); */
            /* set_bt_wl_rab_wl2ext(RF_TX_LDO, RF_TX_EN); */
        }

        SFR(P11_LPCTM0->SLEEP_CON, 3, 2, 0b11); //sleep en
        SFR(P11_LPCTM0->CON0, 2, 1, 1);      //LPCTM WKUP en
        SFR(P11_LPCTM0->CON0, 4, 1, 1);      //模拟滤波使能
        SFR(P11_LPCTM0->CON0, 5, 1, 0);      //连续轮询关闭
        SFR(P11_LPCTM0->CON0, 1, 1, 0);      //模块的中断不使能
        SFR(P11_LPCTM0->CON0, 0, 1, 1);      //模块总开关

        lpctmu_vsel_isel_trim();

        SFR(P11_LPCTM0->ANA0, 0, 1, 0);
        SFR(P11_LPCTM0->ANA1, 3, 1, 0);
        SFR(P11_LPCTM0->ANA1, 2, 1, 0);

        if (__this->ch_ear_en) {
            SFR(P11_LPCTM0->CHEN, 0, 5, __this->ch_key_en);
        } else {
            SFR(P11_LPCTM0->CHEN, 0, 5, __this->ch_en);
            lpctmu_dma_init();
        }

    } else {

        if (__this->ch_ear_en == 0) {
            lpctmu_move_dma_data_to_res_kfifo();

            P11_LPCTM0->WCON |= BIT(5);     //clr dma full pnd
            P11_LPCTM0->WCON |= BIT(4);     //clr dma half pnd
            __asm__ volatile("csync");
            SFR(P11_LPCTM0->DMA_CON, 3, 1, 1);//DMA HALF IE en
            SFR(P11_LPCTM0->DMA_CON, 2, 1, 1);//DMA FULL IE en
        }
    }

    SFR(P11_SYSTEM->P2M_INT_IE, 0, 1, 1);
    SFR(P11_SYSTEM->P11_SYS_CON1, 16, 1, 1);
    request_irq(45, 5, lpctmu_isr, 0);//注册中断函数

    SFR(P11_LPCTM0->WCON, 6, 1, 1);     //clear pnd
    SFR(P11_LPCTM0->CON0, 0, 1, 1);     //模块总开关

    if (__this->ch_ear_en) {
        SFR(P11_LPCTM0->CON0, 1, 1, 1);      //模块的中断使能
        SFR(P11_LPCTM0->CON0, 3, 1, 0);     //关闭由p33 timer 硬件触发采集
        lpctmu_lptimer_init(__this->pdata->sample_scan_time, 1);
        SFR(P11_SYSTEM->P11_SYS_CON0, 19, 1, 1);
        SFR(P11_SYSTEM->P11_SYS_CON0,  5, 1, 1);
        JL_PMU->PMU_CON |= BIT(8);
        request_irq(56, 5, lpctmu_lptimer_isr, 0);//注册中断函数
    } else {
        SFR(P11_LPCTM0->CON0, 3, 1, 1);     //使能由p33 timer 硬件触发采集
        lpctmu_lptimer_init(__this->pdata->sample_scan_time, 0);
    }
    /* SFR(P11_LPCTM0->WCON, 0, 1, 1);     //kick */

    lpctmu_dump();
}

u32 lpctmu_is_working(void)
{
    if (P11_LPCTM0->CON0 & BIT(0)) {
        return 1;
    }
    return 0;
}

void lpctmu_disable(void)
{
    if (!__this) {
        return;
    }
    if (lpctmu_is_working()) {
        if ((__this->ch_num > 1) && (__this->ch_ear_en == 0)) {
            u32 cnt = 0;
            u32 dma_base_offset = __this->ch_num * 4;
            u32 cur_buf_in = (P11_LPCTM0->DMA_WADR / 2) - (dma_base_offset / 2);
            while (lpctmu_addr_to_ch_order(cur_buf_in)) {
                log_debug("wadr:%d, buf_in:%d, cnt:%d", P11_LPCTM0->DMA_WADR, cur_buf_in, cnt);
                mdelay(3);
                cur_buf_in = (P11_LPCTM0->DMA_WADR / 2) - (dma_base_offset / 2);
                cnt ++;
                if (cnt > 10) {
                    break;
                }
            }
        }
        lpctmu_lptimer_disable();
        if (__this->ch_ear_en) {
            u32 cnt = 0;
            while (1) {
                if ((__this->ear_sample_dis) && (__this->sample_cnt >= __this->ch_key_num)) {
                    break;
                } else if (__this->sample_cnt >= __this->sample_cnt_max) {
                    break;
                }
                mdelay(3);
                cnt ++;
                if (cnt > 10) {
                    break;
                }
            }
        }
        SFR(P11_LPCTM0->CON0, 0, 1, 0);     //模块总开关
    }
}

void lpctmu_enable(void)
{
    if (!__this) {
        return;
    }
    if (0 == lpctmu_is_working()) {
        SFR(P11_LPCTM0->WCON, 2, 5, 0x1f);  //clear all pnd
        SFR(P11_LPCTM0->CON0, 0, 1, 1);     //模块总开关
        if (__this->ch_ear_en) {
            lpctmu_lptimer_init(__this->pdata->sample_scan_time, 1);
        } else {
            lpctmu_lptimer_init(__this->pdata->sample_scan_time, 0);
        }
    }
}

u32 lpctmu_is_sf_keep(void)
{
    if (!__this) {
        return 0;
    }

    u32 lpctmu_softoff_keep_work;

    if (lpctmu_is_working()) {
        switch (__this->softoff_wakeup_cfg) {
        case LPCTMU_WAKEUP_DISABLE:
            lpctmu_softoff_keep_work = 0;
            break;
        case LPCTMU_WAKEUP_EN_WITHOUT_CHARGE_ONLINE:
            lpctmu_softoff_keep_work = 1;
            if (get_charge_online_flag()) {
                lpctmu_softoff_keep_work = 0;
            }
            break;
        case LPCTMU_WAKEUP_EN_ALWAYS:
            lpctmu_softoff_keep_work = 1;
            break;
        default :
            lpctmu_softoff_keep_work = 1;
            break;
        }
        if (__this->ch_ear_en) {
            lpctmu_softoff_keep_work = 0;
        }
        if (lpctmu_softoff_keep_work == 0) {
            lpctmu_disable();
            return 0;
        } else {
            return 1;
        }
    }
    return 0;
}

void lpctmu_lowpower_enter(u32 lowpwr_mode)
{
    if (!__this) {
        return;
    }
    if (0 == lpctmu_is_working()) {
        return;
    }

    if (__this->ch_ear_en) {
        return;
    }

    lpctmu_move_dma_data_to_res_kfifo();
    SFR(P11_LPCTM0->DMA_CON, 2, 1, 0);  //DMA FULL IE dis
    SFR(P11_LPCTM0->DMA_CON, 3, 1, 0);  //DMA HALF IE dis
    P11_LPCTM0->WCON |= BIT(5);         //clr dma full pnd
    P11_LPCTM0->WCON |= BIT(4);         //clr dma half pnd
    __asm__ volatile("csync");
    if (lowpwr_mode) {                  //软关机，需要修改lptimer的时间
        SFR(P11_SYSTEM->P2M_INT_IE, 0, 1, 0);
        SFR(P11_SYSTEM->P11_SYS_CON1, 16, 1, 0);
        lpctmu_lptimer_init(__this->pdata->lowpower_sample_scan_time, 0);
    }
}

void lpctmu_lowpower_exit(u32 lowpwr_mode)
{
    if (!__this) {
        return;
    }
    if (0 == lpctmu_is_working()) {
        return;
    }

    if (__this->ch_ear_en) {
        return;
    }

    lpctmu_move_dma_data_to_res_kfifo();
    P11_LPCTM0->WCON |= BIT(5);         //clr dma full pnd
    P11_LPCTM0->WCON |= BIT(4);         //clr dma half pnd
    __asm__ volatile("csync");
    SFR(P11_LPCTM0->DMA_CON, 3, 1, 1);  //DMA HALF IE en
    SFR(P11_LPCTM0->DMA_CON, 2, 1, 1);  //DMA FULL IE en
    if (lowpwr_mode) {                  //软关机，需要修改lptimer的时间
        SFR(P11_SYSTEM->P2M_INT_IE, 0, 1, 1);
        SFR(P11_SYSTEM->P11_SYS_CON1, 16, 1, 1);
        lpctmu_lptimer_init(__this->pdata->sample_scan_time, 0);
    }
}

