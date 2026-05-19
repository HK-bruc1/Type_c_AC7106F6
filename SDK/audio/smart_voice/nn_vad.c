#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".nn_vad.data.bss")
#pragma data_seg(".nn_vad.data")
#pragma const_seg(".nn_vad.text.const")
#pragma code_seg(".nn_vad.text")
#endif
/*****************************************************************
>file name : nn_vad.c
>create time : Thu 16 Dec 2021 07:19:26 PM CST
*****************************************************************/
#include "app_config.h"
#include "typedef.h"
#include "media/tech_lib/jlsp_vad.h"
#include "fs/sdfile.h"
#include "fs/resfile.h"
#include "nn_vad.h"

#if ((defined TCFG_SMART_VOICE_ENABLE) && TCFG_SMART_VOICE_ENABLE)
struct vad_enc {
    void *vad;
    void *share_buffer;
    void *heap_buffer;
    void *model; //nn网络的buffer
};
#define AUDIO_NN_VAD_PARAM_FILE 	(FLASH_RES_PATH"vad.bin")

void *audio_nn_vad_open(void)
{
    struct vad_enc *vad_enc = (struct vad_enc *)zalloc(sizeof(struct vad_enc));
    if (!vad_enc) {
        printf("[error] zalloc vad_enc failed !!!");
        return NULL;
    }
    RESFILE *fp = NULL;
    int param_len  = 0;

    //===============================//
    //          打开参数文件         //
    //===============================//
#if 1
    fp = resfile_open(AUDIO_NN_VAD_PARAM_FILE);
    if (!fp) {
        printf("[err] open vad.bin fail !!!");
        free(vad_enc);
        return NULL;
    }
    param_len = resfile_get_len(fp);
    printf("param_len %d", param_len);
    if (param_len) {
        vad_enc->model = (char *)zalloc(param_len);
    }
    if (!vad_enc->model) {
        printf("[error] zalloc model failed !!!");
        free(vad_enc);
        resfile_close(fp);
        return NULL;
    }
    /* resfile_seek(fp, ptr, RESFILE_SEEK_SET); */
    int rlen = resfile_read(fp, vad_enc->model, param_len);
    if (rlen != param_len) {
        printf("[error] read vad.bin err !!! %d =! %d", rlen, param_len);
        free(vad_enc);
        resfile_close(fp);
        return NULL;
    }
#else
    vad_enc->model  = zalloc(1024 * 30);
    if (!vad_enc->model) {
        free(vad_enc);
        resfile_close(fp);
        return NULL;
    }
    memcpy(vad_enc->model, jlvad_model, sizeof(jlvad_model));
#endif

    int model_size = 0;
    int heap_size = JLSP_vad_get_heap_size(vad_enc->model, &model_size);
    printf("lib_heap_size %d model_size %d\n", heap_size, model_size);
    vad_enc->heap_buffer = zalloc(heap_size);
    if (!vad_enc->heap_buffer) {
        printf("vad_enc->heap_buffer zalloc failed\n");
        free(vad_enc->heap_buffer);
        vad_enc->heap_buffer = NULL;
    }
    int share_size = 1024 * 10;
    vad_enc->share_buffer = zalloc(share_size);
    if (!vad_enc->share_buffer) {
        printf("ctx->share_buffer zalloc failed\n");
        free(vad_enc->share_buffer);
        vad_enc->share_buffer = NULL;
    }
    vad_enc->vad = JLSP_vad_init(vad_enc->heap_buffer,
                                 heap_size,
                                 vad_enc->share_buffer,
                                 share_size,
                                 vad_enc->model,
                                 model_size);

    if (!vad_enc->vad) {
        goto err;
    }
    JLSP_vad_reset(vad_enc->vad);
    resfile_close(fp);
    return vad_enc;
err:
    if (vad_enc) {
        free(vad_enc);
    }
    resfile_close(fp);
    return NULL;
}

int audio_nn_vad_data_handler(void *vad, void *data, int len)
{
    int out_flag = 0;
    struct vad_enc *vad_enc = (struct vad_enc *)vad;
    if (vad_enc) {
        JLSP_vad_process(vad_enc->vad, (s16 *)data, &out_flag); /*0是非语音，1是检测到语音*/
        return out_flag;
    }

    return 0;
}

void audio_nn_vad_close(void *vad)
{
    struct vad_enc *vad_enc = (struct vad_enc *)vad;

    if (vad_enc->model) {
        JLSP_vad_free(vad_enc->model);
    }
    free(vad_enc);
}

#endif

