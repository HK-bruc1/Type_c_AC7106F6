#ifndef _AUDIOEFFECT_DATATYPE_
#define _AUDIOEFFECT_DATATYPE_

enum PCMDataType {
    DATA_INT_16BIT = 0,
    DATA_INT_32BIT,
    DATA_FLOAT_32BIT
};

typedef struct _af_DataType_ {
    unsigned char IndataBit;//输入数据bit类型
    unsigned char OutdataBit;//输出数据bit类型
    char IndataInc;//输入数据相同通道的下一个点间隔
    char OutdataInc;//输出数据相同通道的下一个点间隔
    char Qval;//位宽
} af_DataType;


enum {
    af_DATABIT_NOTSUPPORT = 0x404   //不支持的PCM数据类型返回错误
};

struct effects_func_api {
    unsigned int(*need_buf)(void *param);
    unsigned int(*tmp_buf_size)(void *param);
    int (*init)(void *ptr, void *param, void *tmpbuf);
    int (*set_tmpbuf)(void *ptr, void *tmpbuf);
    int (*run)(void *ptr, void *indata, void *outdata, int PointsPerChannel);
    int (*update)(void *ptr, void *param);
};

struct hard_eq_func {
    void *(*open)(void *coeff, unsigned char nsection, unsigned int sample_rate, unsigned int ch_num, unsigned int in_mode, unsigned int out_mode);
    void(*run)(void *hdl, void *indata, void *outdata, unsigned int indata_len);
    void(*update)(void *hdl, void *coeff);
    void(*close)(void *hdl);
};

#endif
