
/*
 * Copyright (c) 2025 ELEVOC. All Rights Reserved.
 *
 * This source code is proprietary and confidential to ELEVOC.
 * Unauthorized copying, modification, distribution, or use of this code,
 * in whole or in part, is strictly prohibited without the express written
 * permission of ELEVOC.
 */
#ifndef _ELE_TYPEDEF_H_
#define _ELE_TYPEDEF_H_

#include <stdint.h>

#define ENUM_TO_STR_CASE(x) {case x: return(#x);}

/* signed char type defined */
typedef char ele_char;
/* signed integer type defined */
typedef char ele_int8;
typedef short ele_int16;
typedef int ele_int32;
typedef long long ele_int64;

/* unsigned signed integer type defined */
typedef unsigned char ele_uint8;
typedef unsigned short ele_uint16;
typedef unsigned int ele_uint32;
typedef unsigned long long ele_uint64;

/* float type defined */
typedef float ele_float;
typedef float ele_fp32;
typedef double ele_double;

/* complex type defined */
typedef union                   _ele_cint16 {
    ele_int32        i32;
    struct {
        ele_int16    real;
        ele_int16    imag;
    } i16;
} ele_cint16;

typedef union {
    int i32;
    struct _i16 {
        ele_int16    hi;
        ele_int16    lo;
    } i16;
} ele_i16x2;

typedef union                   _ele_cint32 {
    ele_int64        i64;
    struct {
        ele_int32    real;
        ele_int32    imag;
    } i32;
} ele_cint32;

typedef struct _ele_cfloat {
    ele_float real;
    ele_float imag;
} ele_cfloat;

/* math definition */
#define ELE_PI 3.141592653589793238462643383
#define ELE_LOG10 2.3025850929940456840179914547
#define ELE_LOG2 0.6931471805599453094172321214

/* integer data type limited defined */
#define ELE_MIN_S8 (-127 - 1)
#define ELE_MAX_S8 127
#define ELE_MAX_U8 0xffu

#define ELE_MIN_S16 (ele_int16) - 32768 /* 0x8000 */
#define ELE_MAX_S16 (ele_int16) + 32767 /* 0x7fff */
#define ELE_MAX_U16 (ele_uint16)0xffffu

#define ELE_MIN_S32 (ele_int32)0x80000000L
#define ELE_MAX_S32 (ele_int32)0x7fffffffL
#define ELE_MAX_U32 (ele_uint32)0xffffffffU

#define ELE_MIN_S64 (ele_int64)0x8000000000000000LL
#define ELE_MAX_S64 (ele_int64)0x7fffffffffffffffLL
#define ELE_MAX_U64 (ele_uint64)0xffffffffffffffffULL

#define ELE_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ELE_MAX(a, b) (((a) > (b)) ? (a) : (b))

/* data type convert macro between float type and fix type */
#define float2fix(x, Q) (((ele_float)(x)) * (ele_float)(1UL << (Q)))
#define fix2float(x, Q) (((ele_float)(x)) / ((ele_float)(1UL << (Q))))

#define double2fix(x, Q) ((x) * (ele_double)(1ULL << (Q)))
#define fix2double(x, Q) (（(ele_double)(x)) / (ele_double)(1ULL << (Q)))

/* error enum type defined */
typedef ele_int32 ele_state;
#define ELE_NO_ERROR 0x00000000
#define ELE_ERROR 0x80000000
#define ELE_RUN_STATE 0x00000002
#define ELE_IDLE_STATE 0x00000001
#define ELE_AUTH_ERROR (ELE_ERROR | 0x0001)
#define ELE_STREAM_ERROR (ELE_ERROR | 0x0002)
#define ELE_MEMORY_ERROR (ELE_ERROR | 0x0004)
#define ELE_MODEL_ERROR (ELE_ERROR | 0x0008)
#define ELE_PARAM_ERROR (ELE_ERROR | 0x0010)
#define ELE_SERVICE_ERROR (ELE_ERROR | 0x0020)
#define ELE_THREAD_ERROR (ELE_ERROR | 0x0040)
#define ELE_FILE_ERROR (ELE_ERROR | 0x0080)
#define ELE_UNDEF_ERROR (ELE_ERROR | 0x0100)
#define ELE_NULLPTR_ERROR (ELE_ERROR | 0x0200)
#define ELE_NOT_FOUND (ELE_ERROR | 0x0400)
#define ELE_NOT_SUPPORTED (ELE_ERROR | 0x0800)
#define ELE_STATE_ERROR (ELE_ERROR | 0x1000)

/* data enum type defined */
typedef ele_uint16 ele_dtype;
enum {
    ELE_INT8 = 0x100,
    ELE_UINT8,
    ELE_CHAR,
    ELE_INT16 = 0x200,
    ELE_UINT16,
    ELE_FP16,

    ELE_INT24 = 0x300,
    ELE_UINT24,

    ELE_INT32 = 0x400,
    ELE_UINT32,
    ELE_CINT16,

    ELE_FLOAT,
    ELE_FP32 = ELE_FLOAT,
    ELE_TAB_IX,

    ELE_INT64 = 0x800,
    ELE_UINT64,
    ELE_CINT32,
    ELE_DOUBLE,
    ELE_FP64 = ELE_DOUBLE,
    ELE_CFLOAT,
    ELE_CFP32 = ELE_CFLOAT,
};

/* data type sizeof compute macro */
#define dtype_sizeof(x) (ele_uint32)((x) >> 8)
#define DTYPE_SIZEOF(x) (ele_uint32)((x) >> 8)

#ifdef ELEVOC_DEBUG
static inline const char *ele_dtype_to_string(ele_dtype dtype)
{
    switch (dtype) {
        ENUM_TO_STR_CASE(ELE_CHAR);
        ENUM_TO_STR_CASE(ELE_INT8);
        ENUM_TO_STR_CASE(ELE_UINT8);
        ENUM_TO_STR_CASE(ELE_INT16);
        ENUM_TO_STR_CASE(ELE_UINT16);
        ENUM_TO_STR_CASE(ELE_INT32);
        ENUM_TO_STR_CASE(ELE_UINT32);
        ENUM_TO_STR_CASE(ELE_FLOAT);
        ENUM_TO_STR_CASE(ELE_CINT16);
        ENUM_TO_STR_CASE(ELE_INT64);
        ENUM_TO_STR_CASE(ELE_UINT64);
        ENUM_TO_STR_CASE(ELE_DOUBLE);
        ENUM_TO_STR_CASE(ELE_CINT32);
        ENUM_TO_STR_CASE(ELE_CFLOAT);
    default:
        return (const char *)"unknown data type";
    }
}
#endif

typedef ele_uint16 ele_mtype;
enum {
    ELE_HEAP_MEM = 0x1,
    ELE_STACK_MEM,
};

typedef struct _ele_buf {
    void            *data;
    ele_uint32      size;
    ele_dtype       dtype;
    ele_int16       scale;
} ele_buf;

typedef struct ele_io_attr {
    const char *name;
    ele_uint32 size;
    ele_dtype dtype;
    ele_int16 scale;
    ele_uint16 index;
    ele_uint16 ndim;
    ele_int32 *shape;
} ele_io_attr;

#endif /* _ELE_TYPEDEF_H_ */
