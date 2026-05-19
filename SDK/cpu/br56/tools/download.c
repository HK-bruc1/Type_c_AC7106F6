// *INDENT-OFF*
#include "app_config.h"

#ifdef __SHELL__


cd "$(dirname "$0")"

${OBJDUMP} -D -address-mask=0x7ffffff -print-imm-hex -print-dbg -mcpu=r3 $1.elf > $1.lst
${OBJCOPY} -O binary -j .text $1.elf text.bin
${OBJCOPY} -O binary -j .data  $1.elf data.bin
${OBJCOPY} -O binary -j .data_code $1.elf data_code.bin
${OBJCOPY} -O binary -j .data_code_z $1.elf data_code_z.bin
${OBJCOPY} -O binary -j .overlay_init $1.elf init.bin
${OBJCOPY} -O binary -j .overlay_aec $1.elf aec.bin
${OBJCOPY} -O binary -j .overlay_aac $1.elf aac.bin
${OBJCOPY} -O binary -j .dlog_data $1.elf dlog.bin

bank_files=
for i in $(seq 0 20)
do
    ${OBJCOPY} -O binary -j .overlay_bank$i $1.elf bank$i.bin
    if [ -s bank$i.bin ]; then
        bank_files=$bank_files"bank$i.bin 0xbbaa "
    fi
done
#ifdef CONFIG_LZMA_COMPRESS_ENABLE
compress_tool=./lzma_enc
#else
compress_tool=lz4_packet
#endif

$compress_tool -dict text.bin -input data_code_z.bin 0 init.bin 0 aec.bin 0 aac.bin 0 $bank_files -o compress.bin

cat text.bin data.bin data_code.bin compress.bin > app.bin

rm $bankfiles data_code_z.bin text.bin data.bin compress.bin

#if TCFG_TONE_EN_ENABLE
export TONE_EN_ENABLE=1
#else
export TONE_EN_ENABLE=0
#endif

#if TCFG_TONE_ZH_ENABLE
export TONE_ZH_ENABLE=1
#else
export TONE_ZH_ENABLE=0
#endif

#if RCSP_MODE
export RCSP_EN=1
#endif

#if TCFG_UPDATE_COMPRESS
export UPDATE_COMPRESS_ENABLE=1
#else
export UPDATE_COMPRESS_ENABLE=0
#endif

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
if [ ! -d download/earphone/ALIGN_DIR ]; then
    mkdir download/earphone/ALIGN_DIR
fi
cp anc_ext.bin download/earphone/ALIGN_DIR/
#else
if [ -f download/earphone/ALIGN_DIR/anc_ext.bin ]; then
    rm download/earphone/ALIGN_DIR/anc_ext.bin
fi
#endif

if [ "$cibuild" = "y" ]; then
    if [ ! -x isd_download ]; then
        chmod a+x isd_download fw_add json_to_res packres ufw_maker download/earphone/download.sh
    fi
    ./download/earphone/download.sh
else
    ${OBJDUMP} -section-headers -address-mask=0x7ffffff $1.elf > segment_list.txt

    ${OBJSIZEDUMP} -lite -skip-zero -enable-dbg-info $1.elf | sort -k 1 >  symbol_tbl.txt

    /opt/utils/report_segment_usage --sdk_path ${ROOT} \
    --tbl_file symbol_tbl.txt \
    --seg_file segment_list.txt \
    --map_file $1.map \
    --module_depth "{\"apps\":1,\"lib\":2,\"[lib]\":2}"

    cat segment_list.txt

    /opt/utils/strip-ini -i isd_config.ini -o isd_config.ini

    files="app.bin  br56loader.uart br56loader.bin uboot.boot ota*.bin isd_config.ini dlog.bin"

    host-client -project ${NICKNAME}$2 -f ${files} $1.elf
fi

#else

@echo off
Setlocal enabledelayedexpansion
@echo ********************************************************************************
@echo           SDK BR56
@echo ********************************************************************************
@echo %date%


cd /d %~dp0

set OBJDUMP=C:\JL\pi32\bin\llvm-objdump.exe
set OBJCOPY=C:\JL\pi32\bin\llvm-objcopy.exe
set ELFFILE=sdk.elf
#ifdef CONFIG_LZMA_COMPRESS_ENABLE
set compress_tool=.\lzma_enc.exe
#else
set compress_tool=.\lz4_packet.exe
#endif

REM %OBJDUMP% -D -address-mask=0x1ffffff -print-dbg $1.elf > $1.lst
%OBJCOPY% -O binary -j .text %ELFFILE% text.bin
%OBJCOPY% -O binary -j .data %ELFFILE% data.bin
%OBJCOPY% -O binary -j .data_code %ELFFILE% data_code.bin
%OBJCOPY% -O binary -j .data_code_z %ELFFILE% data_code_z.bin
%OBJCOPY% -O binary -j .overlay_init %ELFFILE% init.bin
%OBJCOPY% -O binary -j .overlay_aec %ELFFILE% aec.bin
%OBJCOPY% -O binary -j .overlay_aac %ELFFILE% aac.bin
%OBJCOPY% -O binary -j .dlog_data %ELFFILE% dlog.bin

for /L %%i in (0,1,20) do (
            %OBJCOPY% -O binary -j .overlay_bank%%i %ELFFILE% bank%%i.bin
                set bankfiles=!bankfiles! bank%%i.bin 0xbbaa
        )


%compress_tool% -dict text.bin -input data_code_z.bin 0 init.bin 0 aec.bin 0 aac.bin 0 !bankfiles! -o compress.bin

%OBJDUMP% -section-headers -address-mask=0x1ffffff %ELFFILE%
REM %OBJDUMP% -t %ELFFILE% >  symbol_tbl.txt

copy /b text.bin + data.bin + data_code.bin + compress.bin app.bin

del !bankfiles! data_code_z.bin text.bin data.bin compress.bin

#if TCFG_TONE_EN_ENABLE
set TONE_EN_ENABLE=1
#else
set TONE_EN_ENABLE=0
#endif

#if TCFG_TONE_ZH_ENABLE
set TONE_ZH_ENABLE=1
#else
set TONE_ZH_ENABLE=0
#endif

#if RCSP_MODE
set RCSP_EN=1
#endif

#if TCFG_UPDATE_COMPRESS
set UPDATE_COMPRESS_ENABLE=1
#else
set UPDATE_COMPRESS_ENABLE=0
#endif

#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
copy anc_ext.bin download\earphone\ALIGN_DIR\.
#else
del download\earphone\ALIGN_DIR\anc_ext.bin
#endif

call elf_to_lst.bat
call download/earphone/download.bat

#endif

