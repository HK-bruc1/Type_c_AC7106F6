#include "generic/typedef.h"
#include "asm/math_fast_function.h"

#define NTestData 5

// Define different amplification factors
#define FIX1LSH12 4096
#define FIX1LSH24 16777216
#define FIX1LSH29 536870912
#define FIXNORMALRST24 24
#define FIXNORMALRST15 15

static float bufin[NTestData] = {0};
static float bufin2[NTestData] = {0};
static float bufout_fix[NTestData] = {0};
static float bufout_flo[NTestData] = {0};

extern void put_float(double);

static float Restore2Float(long Indata, char q)
{
    float rst;
    long long tmp1 = 1;
    if (q >= 0) {
        tmp1 = tmp1 << q;
        rst = (float)Indata / (float)tmp1;
    } else {
        tmp1 = tmp1 << -q;
        rst = (float)Indata * (float)tmp1;
    }

    return rst;
}

/**************************
 *
 * API for call Fix_Math
 *
 * ***********************/
static void Math_call_HWAPI(int mode)
{
    int NData = NTestData;

    long tmp1, rst;
    struct data_q_struct datain;
    struct data_q_struct dataout;

    switch (mode) {
    case 1:
        printf("=++++ call sin HW_API  \n");
        for (int j = 0; j < NData; j++) {
            tmp1 = (long)(bufin[j] * FIX1LSH24);
            rst = sin_fix(tmp1);
            bufout_fix[j] = (float)(rst) / FIX1LSH29;
            bufout_flo[j] = sin_float(bufin[j]);
        }

        printf("==== sin Test end  \n");
        break;
    case 2:
        printf("=++++ call cos HW_API  \n");
        for (int j = 0; j < NData; j++) {
            tmp1 = (long)(bufin[j] * FIX1LSH24);
            rst = cos_fix(tmp1);
            bufout_fix[j] = (float)(rst) / FIX1LSH29;
            bufout_flo[j] = cos_float(bufin[j]);
        }

        printf("==== cos Test end  \n");
        break;
    case 3:
        printf("=++++ call cosh HW_API  \n");
        for (int j = 0; j < NData; j++) {
            tmp1 = (long)(bufin[j] * FIX1LSH24);
            rst = cosh_fix(tmp1);
            bufout_fix[j] = (float)(rst) / FIX1LSH29;
            bufout_flo[j] = cosh_float(bufin[j]);
        }

        printf("==== cosh Test end  \n");
        break;
    case 4:
        printf("=++++ call sinh HW_API  \n");
        for (int j = 0; j < NData; j++) {
            tmp1 = (long)(bufin[j] * FIX1LSH24);
            rst = sinh_fix(tmp1);
            bufout_fix[j] = (float)(rst) / FIX1LSH29;
            bufout_flo[j] = sinh_float(bufin[j]);
        }

        printf("==== sinh Test end  \n");
        break;

    case 5:
        printf("=++++ call root HW_API  \n");
        for (int j = 0; j < NData; j++) {
            datain.data = (long)(bufin[j] * FIX1LSH12);
            datain.q = 12;
            dataout = root_fix(datain);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = root_float(bufin[j]);
        }

        printf("==== root Test end  \n");
        break;

    case 6:
        printf("=++++ call exp HW_API  \n");
        for (int j = 0; j < NData; j++) {
            tmp1 = (long)(bufin[j] * FIX1LSH24);
            dataout = exp_fix(tmp1);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = exp_float(bufin[j]);
        }

        printf("==== exp Test end  \n");
        break;
    case 7:
        printf("=++++ call ln HW_API  \n");
        for (int j = 0; j < NData; j++) {
            datain.data = (long)(bufin[j] * FIX1LSH12);
            datain.q = 12;
            dataout = ln_fix(datain);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = ln_float(bufin[j]);
        }

        printf("==== ln Test end  \n");
        break;
    case 8:
        printf("=++++ call log10 HW_API  \n");
        for (int j = 0; j < NData; j++) {
            datain.data = (long)(bufin[j] * FIX1LSH12);
            datain.q = 12;
            dataout = log10_fix(datain);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = log10_float(bufin[j]);
        }

        printf("==== log10 Test end  \n");
        break;
    case 9:
        printf("=++++ call sigmoid HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = sigmoid_fix(bufin[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = sigmoid_float(bufin[j]);
        }

        printf("==== sigmoid Test end  \n");
        break;
    case 10:
        printf("=++++ call tanh HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = tanh_fix(bufin[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = tanh_float(bufin[j]);
        }

        printf("==== tanh Test end  \n");
        break;
    case 11:
        printf("=++++ call complex_abs HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = complex_abs_fix((long)bufin[j], (long)bufin2[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = complex_abs_float(bufin[j], bufin2[j]);
        }

        printf("==== complex_abs Test end  \n");
        break;
    case 12:
        printf("=++++ call angle HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = angle_fix((long)bufin[j], (long)bufin2[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = angle_float(bufin[j], bufin2[j]);
        }

        printf("==== angle Test end  \n");
        break;
    case 13:
        printf("=++++ call mul HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = mul_fix((long)bufin[j], (long)bufin2[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = mul_float(bufin[j], bufin2[j]);
        }

        printf("==== mul Test end  \n");
        break;
    case 14:
        printf("=++++ call div HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = div_fix((long)bufin[j], (long)bufin2[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = div_float(bufin[j], bufin2[j]);
        }

        printf("==== div Test end  \n");
        break;
    case 15:
        printf("=++++ call complex_dqdt HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = complex_dqdt_fix((long)bufin[j], (long)bufin2[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = complex_dqdt_float(bufin[j], bufin2[j]);
        }

        printf("==== complex_dqdt Test end  \n");
        break;
    case 16:
        printf("=++++ call atanh HW_API  \n");
        for (int j = 0; j < NData; j++) {
            dataout = atanh_fix((long)bufin[j], (long)bufin2[j]);
            bufout_fix[j] = Restore2Float(dataout.data, dataout.q);
            bufout_flo[j] = atanh_float(bufin[j], bufin2[j]);
        }

        printf("==== atanh Test end  \n");
        break;
    case 17:
        printf("=++++ call add HW_API  \n");
        for (int j = 0; j < NData; j++) {
            bufout_flo[j] = add_float(bufin[j], bufin2[j]);
        }

        printf("=++= float_add Test end  \n");
        break;
    case 18:
        printf("=++++ call sub HW_API  \n");
        for (int j = 0; j < NData; j++) {
            bufout_flo[j] = sub_float(bufin[j], bufin2[j]);
        }

        printf("=++= float_sub Test end  \n");
        break;
    case 19:
        printf("=++++ call dB2mag HW_API  \n");
        for (int j = 0; j < NData; j++) {
            bufout_flo[j] = dB_Convert_Mag(bufin[j]);
        }

        printf("=++= float_dB2mag Test end  \n");
        break;
    default:
        printf("no such case !! ");
        break;
    }
}

//=======================================================================
// call API for give a typical example
//=======================================================================
enum mode_select {
    fixFlo_sin = 1,
    fixFlo_cos,
    fixFlo_cosh,
    fixFlo_sinh,
    fixFlo_root,
    fixFlo_exp,
    fixFlo_ln,
    fixFlo_log10,
    fixFlo_sigmoid,
    fixFlo_tanh,
    fixFlo_complex_abs,
    fixFlo_angle,
    fixFlo_mul,
    fixFlo_div,
    fixFlo_complex_dqdt,
    fixFloFlo_atanh,
    float_add,
    float_sub,
    float_dB2mag,

};

//=========================================
//    main test API
//=========================================
void UseDemoTest_main(void)
{
    float tmp0 = 0;
#if 1
    /***************************************
     *   Call sin API
     * random input:   0、0.4pi、0.8pi、1.2pi、1.6pi
     * * The actual input does not contain pi
     * output:         0、0.951、0.587、-0.587、-0.951
     * *************************************/
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 0.4f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_sin);
    for (int i = 0; i < NTestData; i++) {
        printf("-->sin_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->sin_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->sin_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call cos API
     * random input:   0、0.6pi、1.2pi、1.8pi、2.4pi
     * * The actual input does not contain pi
     * output:         1、-0.309、-0.809、0.809、0.309
     * *************************************/
    tmp0 = 0;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 0.6f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_cos);
    for (int i = 0; i < NTestData; i++) {
        printf("-->cos_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->cos_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->cos_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call cosh API
     * random input:   -0.5、-0.3、-0.1、0.1、0.3
     * output:         1.127、1.045、1.005、1.005、1.045
     * *************************************/
    tmp0 = -0.5;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 0.2f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_cosh);
    for (int i = 0; i < NTestData; i++) {
        printf("-->cosh_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->cosh_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->cosh_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call sinh API
     * random input:   -0.6、-0.3、0、0.3、0.6
     * output:         -0.636、-0.304、0、0.304、0.636
     * *************************************/
    tmp0 = -0.6;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 0.3f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_sinh);
    for (int i = 0; i < NTestData; i++) {
        printf("-->sinh_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->sinh_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->sinh_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call root API
     * random input:  1000 、800、600、400、200
     * output:        31.622、28.284、24.494、20、14.142
     * *************************************/
    tmp0 = 1000.f;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 - 200.f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_root);
    for (int i = 0; i < NTestData; i++) {
        printf("-->root_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->root_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->root_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call exp API
     * random input:  -5 、-3、-1、1、3
     * output:        0.006、0.049、0.367、2.718、20.085
     * *************************************/
    tmp0 = -5.f;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 2.f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_exp);
    for (int i = 0; i < NTestData; i++) {
        printf("-->exp_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->exp_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->exp_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call ln API
     * random input:  10.5 、20.5、30.5、40.5、50.5
     * output:        2.351、3.02、3.417、3.701、3.921
     * *************************************/
    tmp0 = 10.5f;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 10.f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_ln);
    for (int i = 0; i < NTestData; i++) {
        printf("-->ln_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->ln_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->ln_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call log10 API
     * random input:  10 、20.5、31、41.5、52
     * output:        0.999、1.311、1.491、1.618、1.716
     * *************************************/
    tmp0 = 10.f;
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmp0;
        tmp0 = tmp0 + 10.5f;
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_log10);
    for (int i = 0; i < NTestData; i++) {
        printf("-->log10_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->log10_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->log10_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call sigmoid API
     * random input:  0.3232, 0.9697, 1.6162, 2.2626, 2.9091
     * output:        0.58、0.725、0.834、0.905、0.948
     * *************************************/
    float tmpbufin_0[NTestData] = {
        0.3232f,
        0.9697f,
        1.6162f,
        2.2626f,
        2.9091f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_0[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_sigmoid);
    for (int i = 0; i < NTestData; i++) {
        printf("-->sigmoid_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->sigmoid_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->sigmoid_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call tanh API
     * random input:  0.3232, 0.9697, 1.6162, 2.2626, 2.9091
     * output:        0.312、0.748、0.924、0.798、0.994
     * *************************************/
    float tmpbufin_1[NTestData] = {
        0.3232f,
        0.9697f,
        1.6162f,
        2.2626f,
        2.9091f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_1[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_tanh);
    for (int i = 0; i < NTestData; i++) {
        printf("-->tanh_float_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->tanh_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->tanh_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call complex_abs API
     * random input x:  46.3238, 55.6060, 11.3069, 25.3331, 25.7553,
     * random input y:  0.7564, 34.8477, 25.5522, 19.7835, 31.0797,
     * output:          46.329、65.623、27.942、32.142、40.364
     * *************************************/
    float tmpbufin_00[NTestData] = {
        46.3238f,
        55.6060f,
        11.3069f,
        25.3331f,
        25.7553f
    };
    float tmpbufin_01[NTestData] = {
        0.7564f,
        34.8477f,
        25.5522f,
        19.7835f,
        31.0797f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_00[i];
        bufin2[i] = tmpbufin_01[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_complex_abs);
    for (int i = 0; i < NTestData; i++) {
        printf("-->complex_abs_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->complex_abs_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->complex_abs_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->complex_abs_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call angle API
     * random input x:  51.0671, -57.4870, -26.6209, -56.0094, -51.8920,
     * random input y:  31.4161, -19.6922, -29.4006, -33.4671, 44.3438,
     * output:          0.175、-0.897、-0.734、-0.828、0.774
     * *************************************/
    float tmpbufin_02[NTestData] = {
        51.0671f,
        -57.4870f,
        -26.6209f,
        -56.0094f,
        -51.8920f
    };
    float tmpbufin_03[NTestData] = {
        31.4161f,
        -19.6922f,
        -29.4006f,
        -33.4671f,
        44.3438f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_02[i];
        bufin2[i] = tmpbufin_03[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_angle);
    for (int i = 0; i < NTestData; i++) {
        printf("-->angle_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->angle_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->angle_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->angle_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call mul API
     * random input x:  6.6077, 5.5130, 35.6830, 58.7717, 3.9159,
     * random input y:  37.2187, 55.4998, 53.3945, 9.2461, 3.3953,
     * output:          245.93、305.97、1905.275、543.409、13.295
     * *************************************/
    float tmpbufin_04[NTestData] = {
        6.6077f,
        5.5130f,
        35.6830f,
        58.7717f,
        3.9159f
    };
    float tmpbufin_05[NTestData] = {
        37.2187f,
        55.4998f,
        53.3945f,
        9.2461f,
        3.3953f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_04[i];
        bufin2[i] = tmpbufin_05[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_mul);
    for (int i = 0; i < NTestData; i++) {
        printf("-->mul_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->mul_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->mul_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->mul_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call div API
     * random input x:  62.4981, 21.2741, 35.6666, 49.2727, 35.2512,
     * random input y:  34.9541, 53.8234, 53.1533, 25.0416, 16.8357,
     * output:          0.559、2.529、1.49、0.508、0.477
     * *************************************/
    float tmpbufin_06[NTestData] = {
        62.4981f,
        21.2741f,
        35.6666f,
        49.2727f,
        35.2512f
    };
    float tmpbufin_07[NTestData] = {
        34.9541f,
        53.8234f,
        53.1533f,
        25.0416f,
        16.8357f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_06[i];
        bufin2[i] = tmpbufin_07[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_div);
    for (int i = 0; i < NTestData; i++) {
        printf("-->div_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->div_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->div_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->div_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call complex_dqdt API
     * Input data restriction:  1/32 < y/x < 1/2
     * random input x:  37.5678, 19.9703, 52.7003, 21.0681, 35.7866,
     * random input y:  1.1778, 0.7811, 2.4704, 1.1511, 2.2332,
     * output:          37.549、19.955、52.642、21.036、35.716
     * *************************************/
    float tmpbufin_08[NTestData] = {
        37.5678f,
        19.9703f,
        52.7003f,
        21.0681f,
        35.7866f
    };
    float tmpbufin_09[NTestData] = {
        1.1778f,
        0.7811f,
        2.4704f,
        1.1511f,
        2.2332f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_08[i];
        bufin2[i] = tmpbufin_09[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFlo_complex_dqdt);
    for (int i = 0; i < NTestData; i++) {
        printf("-->complex_dqdt_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->complex_dqdt_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->complex_dqdt_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->complex_dqdt_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call atanh API
     * Input data restriction:  1/32 < y/x < 1/2
     * random input x:  7.8074, 30.5401, 36.5852, 26.6999, 27.8781,
     * random input y:  0.2448, 10.4409, 15.0637, 12.0299, 20.3519,
     * output:          0.031、0.346、0.437、0.485、0.928
     * *************************************/
    float tmpbufin_10[NTestData] = {
        7.8074f,
        30.5401f,
        36.5852f,
        26.6999f,
        27.8781f
    };
    float tmpbufin_11[NTestData] = {
        0.2448f,
        10.4409f,
        15.0637f,
        12.0299f,
        20.3519f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_10[i];
        bufin2[i] = tmpbufin_11[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(fixFloFlo_atanh);
    for (int i = 0; i < NTestData; i++) {
        printf("-->atanh_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->atanh_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->atanh_fix_out: ");
        put_float(bufout_fix[i]);
        printf("\n");
        printf("-->atanh_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call add API
     * random input x:  705.1905, 776.5554, 256.0102, -78.5782, 394.7200,
     * random input y:  -868.0394, 26.5937, -870.8639, 836.0366, 6.6809,
     * output:          -162.848、803.149、-614.853、757.458、401.4
     * *************************************/
    float tmpbufin_12[NTestData] = {
        705.1905f,
        776.5554f,
        256.0102f,
        -78.5782f,
        394.7200f
    };
    float tmpbufin_13[NTestData] = {
        -868.0394f,
        26.5937f,
        -870.8639f,
        836.0366f,
        6.6809f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_12[i];
        bufin2[i] = tmpbufin_13[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(float_add);
    for (int i = 0; i < NTestData; i++) {
        printf("-->add_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->add_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->add_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call sub API （y-x）
     * input x:         -704.8555, 47.9364, -492.9296, -860.9529, -930.6488,
     * random input y:  205.4035, -918.6968, 404.7593, 563.6365, 650.7724,
     * output:          910.259、-966.633、897.688、1424.589、1581.421
     * *************************************/
    float tmpbufin_14[NTestData] = {
        -704.8555f,
        47.9364f,
        -492.9296f,
        -860.9529f,
        -930.6488f
    };
    float tmpbufin_15[NTestData] = {
        205.4035f,
        -918.6968f,
        404.7593f,
        563.6365f,
        650.7724f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_14[i];
        bufin2[i] = tmpbufin_15[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(float_sub);
    for (int i = 0; i < NTestData; i++) {
        printf("-->sub_input_X: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->sub_input_Y: ");
        put_float(bufin2[i]);
        printf("\n");
        printf("-->sub_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif

#if 1
    /***************************************
     *   Call float_dB2mag API
     * random input :  100, 50, 30, 10, -20,
     * output:         100000.056、316.227、31.622、3.162、0.099
     * *************************************/
    float tmpbufin_16[NTestData] = {
        100.f,
        50.f,
        30.f,
        10.f,
        -20.f
    };
    for (int i = 0; i < NTestData; i++) {
        bufin[i] = tmpbufin_16[i];
        bufout_fix[i] = 0.f;
        bufout_flo[i] = 0.f;
    }
    Math_call_HWAPI(float_dB2mag);
    for (int i = 0; i < NTestData; i++) {
        printf("-->dB2mag_input: ");
        put_float(bufin[i]);
        printf("\n");
        printf("-->dB2mag_float_out: ");
        put_float(bufout_flo[i]);
        printf("\n");
    }
    printf("-----------------------------\n");
#endif
}
