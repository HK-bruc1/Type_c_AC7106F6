#ifndef JLA_LL_CODEC_API_H__
#define JLA_LL_CODEC_API_H__


// enum {
// ERR_NONE = 0,
// ERR_OCCUR = 1,
// };

extern int jla_ll_encoder_ops_run(void *ptr, short indata[], char outdata[], short len, int resetflag);
extern int jla_ll_decoder_ops_run(void *ptr, char indata[], short outdata[], short len, int resetflag, int errflag);
extern int  jla_ll_encoder_needbuf();
extern int  jla_ll_decoder_needbuf();
extern void  jla_ll_encoder_ops_open(void *ptr, int blocklen, int blocklen4);
extern void  jla_ll_decoder_ops_open(void *ptr, int sr, int blocklen, int blocklen4);
extern int  get_jla_ll_len(int pcmpoints, int blocklen4, int bitsPerSample);

int jla_ll_enc_frame_len();

#endif // jla_ll_codec_api_h__

