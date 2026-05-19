
#ifndef AUDIO_EFF_FILE_VERIFY_H
#define AUDIO_EFF_FILE_VERIFY_H

#include "init.h"
#include "os/os_compat.h"
#include "fs/fs.h"
#include "device/device.h"


/*
 *SD卡初始化，以及文件系统挂载
 * */
void eff_dev_init();

/*
 *在根目录创建文件
 *name   :文件命名：8+3的命名 xxxxxxxx.xxx
 *mode   :"w+"(创建文件,以写的方式打开文件), "r"(以读的方式打开文件)
 *return :返回文件句柄，非0创建成功，
 * */
void *eff_fopen(char *name, char *mode);

/*
 *写数据到文件内
 *fp    :eff_fopen返回的指针
 *indata:输入数据地址
 *len   :输入数据长度，单位byte
 *return:返回实际写入的长度
 * */
int eff_fwrite(void *fp, void *indata, int len);


/*
 *从文件内读数据出来
 *fp     :eff_fopen返回的指针
 *outdata:输出数据的地址
 *len    :要读出的数据长度，单位byte
 *return :返回实际读到长度
 * */
int eff_fread(void *fp, void *outdata, int len);

/*
 *关闭文件
 *fp:eff_fopen返回的指针
 * */
int eff_fclose(void *fp);

/*
 *测试例子
 * */
void eff_verify_test();


#endif
