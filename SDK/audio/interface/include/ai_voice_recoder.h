#ifndef AI_VOICE_RECODER_H
#define AI_VOICE_RECODER_H

#define AI_VOICE_FIXED_OPUS_SAMPLE_RATE     16000
#define AI_VOICE_FIXED_OPUS_CHANNELS        1
#define AI_VOICE_FIXED_OPUS_BIT_RATE        32000
#define AI_VOICE_FIXED_OPUS_FRAME_MS        20



int ai_voice_recoder_open(u32 code_type, u8 ai_type);

int ai_voice_recoder_open_with_tx(u32 code_type, u8 ai_type,
                                  int (*tx_func)(u8 *, u32));

void ai_voice_recoder_close();

void ai_voice_recoder_set_ai_tx_node_func(int (*func)(u8 *, u32));






#endif
