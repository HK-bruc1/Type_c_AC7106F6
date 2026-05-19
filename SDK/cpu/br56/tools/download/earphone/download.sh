#!/bin/bash

# Linux版本下载脚本

cd "$(dirname "$0")"

# 复制必要的文件
cp ../../anc_coeff.bin .
cp ../../anc_gains.bin .
cp ../../br56loader.bin .
cp ../../ota.bin .

# 处理密钥文件
if [ -n "$KEY_FILE_PATH" ]; then
    KEY_FILE="-key $KEY_FILE_PATH"
else
    KEY_FILE=""
fi

# 设置项目下载路径
if [ -z "$PROJ_DOWNLOAD_PATH" ]; then
    PROJ_DOWNLOAD_PATH="../../../../../../output"
fi

# 复制项目文件
cp $PROJ_DOWNLOAD_PATH/*.bin .
if [ -f "$PROJ_DOWNLOAD_PATH/tone_en.cfg" ]; then
    cp $PROJ_DOWNLOAD_PATH/tone_en.cfg .
fi
if [ -f "$PROJ_DOWNLOAD_PATH/tone_zh.cfg" ]; then
    cp $PROJ_DOWNLOAD_PATH/tone_zh.cfg .
fi


TONE_FILES=""
if [ "$TONE_EN_ENABLE" = "1" ]; then
    if [ ! -f "tone_en.cfg" ]; then
        cp ../../tone.cfg tone_en.cfg
    fi
    TONE_FILES="tone_en.cfg"
fi

if [ "$TONE_ZH_ENABLE" = "1" ]; then
    if [ -z "$TONE_FILES" ]; then
        TONE_FILES="tone_zh.cfg"
    else
        TONE_FILES="$TONE_FILES tone_zh.cfg"
    fi
fi

# 处理格式化选项
FORMAT=""
if [ "$FORMAT_VM_ENABLE" = "1" ]; then
    FORMAT="-format vm"
fi
if [ "$FORMAT_ALL_ENABLE" = "1" ]; then
    FORMAT="-format all"
fi

# 处理RCSP配置
CONFIG_DATA=""
if [ -n "$RCSP_EN" ]; then
    ../../json_to_res ../../json.txt
    CONFIG_DATA="config.dat"
fi

# 执行下载命令
echo "开始下载..."
set -x
../../isd_download ../../isd_config.ini -tonorflash -dev br56 -boot 0x100000 -div8 -wait 300 -uboot ../../uboot.boot -app ../../app.bin -tone $TONE_FILES -res cfg_tool.bin stream.bin $CONFIG_DATA $KEY_FILE $FORMAT -output-ufw update.ufw
set +x

# 清理临时文件
[ -f "*.mp3" ] && rm *.mp3
[ -f "*.PIX" ] && rm *.PIX
[ -f "*.TAB" ] && rm *.TAB
[ -f "*.res" ] && rm *.res
[ -f "*.sty" ] && rm *.sty


if [ "$cibuild" = "y" ]; then
    RELEASE_DIR=../../../../../../release/$cibuild_version
    if [ ! -d $RELEASE_DIR ]; then
        mkdir -p $RELEASE_DIR
    fi
    cp update.ufw $RELEASE_DIR/update.ufw
    cp jl_isd.fw $RELEASE_DIR/jl_isd.fw
fi

