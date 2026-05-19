#ifndef GAIN_MIX_API
#define GAIN_MIX_API

#define GAIN_BIT 15 //定点运算增益放大15bit

#ifdef WIN32

#define AT_GAIN_MIX(x)
#define AT_GAIN_MIX_CODE
#define AT_GAIN_MIX_CONST
#define AT_GAIN_MIX_SPARSE_CODE
#define AT_GAIN_MIX_SPARSE_CONST

#else

#define AT_GAIN_MIX(x)				__attribute__((section(#x)))
#define AT_GAIN_MIX_CODE			AT_GAIN_MIX(.gain_mix.text.cache.L2.code)
#define AT_GAIN_MIX_CONST			AT_GAIN_MIX(.gain_mix.text.cache.L2.const)
#define AT_GAIN_MIX_SPARSE_CODE		AT_GAIN_MIX(.gain_mix.text)
#define AT_GAIN_MIX_SPARSE_CONST	AT_GAIN_MIX(.gain_mix.text.const)

#endif

typedef struct _MixParam {
    void *data;        //ÿ每路数据
    float gain;        //增益 floag gain ; gain = eq_db2mag(gain);
} MixParam;

typedef struct _MixParamFix {
    void *data;
    int gain;  //float ffgain = eq_db2mag(fgain); gain = (int)(ffgain*(1<<GAIN_BIT))
} MixParamFix;
/*
 *16bit位宽，多带数据叠加
 **out_buf_tmp:输出目标地址
 *point_per_channel:每通道点数
 * */
void  band_merging_16bit(short *in0, short *in1, short *in2, short *out_buf_tmp, int points, unsigned char nband);
/*32bit位宽，多带数据叠加
 *out_buf_tmp:输出目标地址
 *point_per_channel:每通道点数
 * */
void  band_merging_32bit(int *in0, int *in1, int *in2, int *out_buf_tmp, int points, unsigned char nband);

void GainProcess_16Bit(short *in, short *out, float gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void GainProcess_32Bit(int *in, int *out, float gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SplitGainProcess_16Bit(short *in, short *out, float *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SplitGainProcess_32Bit(int *in, int *out, float *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SteroMix_16bit(short *in, short *out, float *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SteroMix_32bit(int *in, int *out, float *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void Mix16to16(MixParam *mix1, MixParam *mix2, MixParam *mix3, short *out, int channel, int way_num, int per_channel_npoint);
/*
 *功能：输入数据16bit mix ,输出数据16bit
 *way_num 为2或者3     way_num = 2  取 mix1  mix2的数据，mix3无效
 *out  数据输出  带16bit饱和
 *per_channel_npoint   每个通道的样点数
 * */
void Mix32to32(MixParam *mix1, MixParam *mix2, MixParam *mix3, int *out, int channel, int way_num, int per_channel_npoint);

/*
 *功能： 输入数据32bit mix ,输出数据32bit
 *way_num 为2或者3     way_num = 2  取 mix1  mix2的数据，mix3无效
 *out  数据输出   不带16bit饱和
 *per_channel_npoint   每个通道的样点数
 * */
void Mix32to16(MixParam *mix1, MixParam *mix2, MixParam *mix3, short *out, int channel, int way_num, int per_channel_npoint);

/*
 *功能： 输入数据32bit mix ,输出数据16bit
 *way_num 为2或者3     way_num = 2  取 mix1  mix2的数据，mix3无效
 *out  数据输出   带16bit饱和
 *per_channel_npoint   每个通道的样点数
 * */
void Mix16to32(MixParam *mix1, MixParam *mix2, MixParam *mix3, int *out, int channel, int way_num, int per_channel_npoint);
/*
 *将数据叠加到输出buf
 *mix:输入数据结构
 *out:输出数据地址
 *npoint:总的点数
 * */
void mix_data_16to16(MixParam *mix, short *out, int npoint);
void mix_data_32to32(MixParam *mix, int *out, int npoint);
void mix_data_32to16(MixParam *mix, short *out, int npoint);
void mix_data_16to32(MixParam *mix, int *out, int npoint);

// 定点运算，增益需要放大15BIT 取整后再传入接口
// flaot fgain;
// int gain = (int)(fgain*(1<<GAIN_BIT));
void GainProcess_16BitFix(short *in, short *out, int gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void GainProcess_32BitFix(int *in, int *out, int gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SplitGainProcess_16BitFix(short *in, short *out, int *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SplitGainProcess_32BitFix(int *in, int *out, int *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SteroMix_16bitFix(short *in, short *out, int *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void SteroMix_32bitFix(int *in, int *out, int *gain, int channel, int IndataInc, int OutdataInc, int per_channel_npoint);
void Mix16to16Fix(MixParamFix *mix1, MixParamFix *mix2, MixParamFix *mix3, short *out, int channel, int way_num, int per_channel_npoint);
void Mix32to32Fix(MixParamFix *mix1, MixParamFix *mix2, MixParamFix *mix3, int *out, int channel, int way_num, int per_channel_npoint);
void Mix32to16Fix(MixParamFix *mix1, MixParamFix *mix2, MixParamFix *mix3, short *out, int channel, int way_num, int per_channel_npoint);
void Mix16to32Fix(MixParamFix *mix1, MixParamFix *mix2, MixParamFix *mix3, int *out, int channel, int way_num, int per_channel_npoint);
void mix_data_16to16_fix(MixParamFix *mix, short *out, int npoint);
void mix_data_32to32_fix(MixParamFix *mix, int *out, int npoint);
void mix_data_32to16_fix(MixParamFix *mix, short *out, int npoint);
void mix_data_16to32_fix(MixParamFix *mix, int *out, int npoint);

#endif
