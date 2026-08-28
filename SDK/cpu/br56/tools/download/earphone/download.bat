@echo off

cd %~dp0

copy ..\..\anc_coeff.bin .
copy ..\..\anc_gains.bin .
copy ..\..\br56loader.bin .
copy ..\..\ota.bin .

if not %KEY_FILE_PATH%A==A set KEY_FILE=-key %KEY_FILE_PATH%

if %PROJ_DOWNLOAD_PATH%A==A set PROJ_DOWNLOAD_PATH=..\..\..\..\..\..\output
copy %PROJ_DOWNLOAD_PATH%\*.bin .	
if exist %PROJ_DOWNLOAD_PATH%\tone_en.cfg copy %PROJ_DOWNLOAD_PATH%\tone_en.cfg .	
if exist %PROJ_DOWNLOAD_PATH%\tone_zh.cfg copy %PROJ_DOWNLOAD_PATH%\tone_zh.cfg .
if exist sdk_config.h del sdk_config.h
if exist sdk_config.c del sdk_config.c

:: ��鲢����Ӣ�������ļ�
if %TONE_EN_ENABLE%A==1A (
    if not exist tone_en.cfg copy ..\..\tone.cfg tone_en.cfg
    set TONE_FILE_LIST=tone_en.cfg
)

:: ��鲢�������������ļ�
if %TONE_ZH_ENABLE%A==1A (
    if defined TONE_FILE_LIST (
        set TONE_FILE_LIST=%TONE_FILE_LIST% tone_zh.cfg
    ) else (
        set TONE_FILE_LIST=tone_zh.cfg
    )
)

:: ������ղ���
if defined TONE_FILE_LIST (
    set TONE_FILES=-tone %TONE_FILE_LIST%
)

if %FORMAT_VM_ENABLE%A==1A set FORMAT=-format vm
if %FORMAT_ALL_ENABLE%A==1A set FORMAT=-format all

if not %RCSP_EN%A==A (
   ..\..\json_to_res.exe ..\..\json.txt
    set CONFIG_DATA=config.dat
)


@echo on
..\..\isd_download.exe ..\..\isd_config.ini -tonorflash -dev br56 -boot 0x100000 -div8 -wait 300 -uboot uboot.boot -app ..\..\app.bin %TONE_FILES% -res cfg_tool.bin stream.bin %CONFIG_DATA% %KEY_FILE% -key AC690X-9388.key %FORMAT% -output-ufw update.ufw
@echo off
:: -format all
::-reboot 2500

@rem ɾ����ʱ�ļ�-format all
if exist *.mp3 del *.mp3 
if exist *.PIX del *.PIX
if exist *.TAB del *.TAB
if exist *.res del *.res
if exist *.sty del *.sty

@rem ���ɹ̼������ļ�
::..\..\fw_add.exe -noenc -fw jl_isd.fw -add ..\..\ota.bin -type 100 -out jl_isd.fw
@rem �������ýű��İ汾��Ϣ�� FW �ļ���
::..\..\fw_add.exe -noenc -fw jl_isd.fw -add ..\..\script.ver -out jl_isd.fw

::..\..\ufw_maker.exe -fw_to_ufw jl_isd.fw
::copy jl_isd.ufw update.ufw
::del jl_isd.ufw

copy update.ufw %PROJ_DOWNLOAD_PATH%\update.ufw
copy jl_isd.bin %PROJ_DOWNLOAD_PATH%\jl_isd.bin
copy jl_isd.fw %PROJ_DOWNLOAD_PATH%\jl_isd.fw

if %UPDATE_COMPRESS_ENABLE%A==1A (
    ..\..\isd_download.exe -make-upgrade-bin -ufw update.ufw -output db_update.bin
    ..\..\ufw_maker.exe -chip AC710N -enc -res db_update.bin -output update-com.ufw
    copy update-com.ufw %PROJ_DOWNLOAD_PATH%\update-com.ufw
)

@rem ��������˵��
@rem -format vm        //����VM ����
@rem -format cfg       //����BT CFG ����
@rem -format 0x3f0-2   //��ʾ�ӵ� 0x3f0 �� sector ��ʼ�������� 2 �� sector(��һ������Ϊ16���ƻ�10���ƶ��ɣ��ڶ�������������10����)

ping /n 2 127.1>null
IF EXIST null del null
