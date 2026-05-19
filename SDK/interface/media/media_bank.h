#ifndef MEDIA_BANK_H
#define MEDIA_BANK_H

#define __STREAM_BANK_CODE     __attribute__((section(".jlstream.bank.text")))
#define __VOLUME_BANK_CODE  __attribute__((section(".volume.bank.text")))
#define __EQ_BANK_CODE  __attribute__((section(".eq.bank.text")))
#define __AUDIO_SYNC_BANK_CODE  __attribute__((section(".audio_sync.bank.text")))
#define __DAC_BANK_CODE  __attribute__((section(".dac.bank.text")))
#define __CVP_BANK_CODE  __attribute__((section(".cvp.bank.text")))
#define __A2DP_DEC_BANK_CODE __attribute__((section(".a2dp_dec.bank.text")))
#define __AUDIO_INIT_BANK_CODE __attribute__((section(".audio_init.bank.text")))
#define __ENCODE_BANK_CODE     __attribute__((section(".encode.bank.text")))
#define __AUDIO_ADC_BANK_CODE __attribute__((section(".audio_adc.bank.text")))


#endif
