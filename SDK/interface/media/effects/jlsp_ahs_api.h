#ifndef __JLSP_AHS_API_H__
#define __JLSP_AHS_API_H__

typedef struct {
    float ahs_gain_floor;
    float ns_gain_floor;
    float input_gain;
    float out_gain;
    float rev;
    int flag0;
    int flag1;
    int flag2;
} ahs_param_t;

int jlsp_ahs_set_parameters(void *m, ahs_param_t *param);

int jlsp_ahs_get_heap_size(int *private_buf_size, int *shared_buf_size, int L, int F, float fb_delay);
void *jlsp_ahs_init(char *private_buffer, int private_size, char *share_buffer, int share_size, int L, int F, float fb_delay);

int jlsp_ahs_process(void *m, char *near_buf, char *far_buf, char *out, int *outsize);
int jlsp_ahs_process0(void *m, char *near_buf, char *far_buf, char *out, int *outsize);
int jlsp_ahs_process1(void *m, char *near_buf, char *far_buf, char *out, int *outsize);
int jlsp_ahs_free(void *m);


#endif
