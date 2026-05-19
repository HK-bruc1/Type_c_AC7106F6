#include "audio_effect_verify.h"

#if defined(CONFIG_FATFS_ENABLE) && CONFIG_FATFS_ENABLE

#define SD_DEV 	"sd0"
#define SD_MOUNT_PATH "storage/sd0"
#define SD_ROOT_PATH "storage/sd0/C/"

extern void force_set_sd_online(char *sdx);

/*
 *文件系统接口测试
 * */
int SD_private_test(void)
{
#define FILE_NAME SD_ROOT_PATH"abc.txt"
    log_d("SD driver test >>>> path: %s", FILE_NAME);

    FILE *fp = NULL;
    u8 str[] = "This is a test string.";
    u8 buf[100];
    u8 len;

    fp = fopen(FILE_NAME, "w+");
    if (!fp) {
        log_d("open file ERR!");
        return -1;
    }

    len = fwrite(str, sizeof(str), 1, fp);

    if (len != sizeof(str)) {
        log_d("write file ERR!");
        goto _end;
    }
#if 1
    fseek(fp, 0, SEEK_SET);

    len = fread(buf, sizeof(str), 1, fp);

    if (len != sizeof(str)) {
        log_d("read file ERR!");
        goto _end;
    }

    put_buf(buf, sizeof(str));
#endif
    log_d("SD ok!");

_end:
    if (fp) {
        fclose(fp);
    }

    return 0;
}

/*
 *SD卡初始化，以及文件系统挂载
 * */
void eff_dev_init()
{
    devices_init();
    force_set_sd_online("sd0");
    void *mnt = mount("sd0", "storage/sd0", "fat", 3, NULL);
    if (!mnt) {
        puts(" ===sd mount err===\n");
    } else {
        puts("===sd mount ok===\n");
    }
}
/*
 *在根目录创建文件
 *name   :文件命名：8+3的命名 xxxxxxxx.xxx
 *mode   :"w+"(创建文件,以写的方式打开文件), "r"(以读的方式打开文件)
 *return :返回文件句柄，非0创建成功，
 * */
void *eff_fopen(char *name, char *mode)
{
    FILE *fp = NULL;
    char path[64];
    sprintf(path, SD_ROOT_PATH"%s", name);
    fp = fopen(path, mode);
    return fp;
}

/*
 *写数据到文件内
 *fp    :eff_fopen返回的指针
 *indata:输入数据地址
 *len   :输入数据长度，单位byte
 *return:返回实际写入的长度
 * */
int eff_fwrite(void *fp, void *indata, int len)
{
    return  fwrite(indata, len, 1, fp);
}
/*
 *从文件内读数据出来
 *fp     :eff_fopen返回的指针
 *outdata:输出数据的地址
 *len    :要读出的数据长度，单位byte
 *return:返回实际读到长度
 * */
int eff_fread(void *fp, void *outdata, int len)
{
    return fread(outdata, len, 1, fp);
}

/*
 *关闭文件
 *fp:eff_fopen返回的指针
 * */
int eff_fclose(void *fp)
{
    return fclose(fp);
}

/*
 *测试例子,可在 app_task_init接口后，调用本测试接口
 * */
void eff_verify_test()
{
    puts("=============eff verify test begin ==================\n");
    //注意以下宏定义的使能，否则无法
    //sdk_config.h内使能宏TCFG_SD0_ENABLE
    //app_config.h内使能CONFIG_FATFS_ENABLE
    //SD卡初始化，以及文件系统挂载
    eff_dev_init();

    /* //文件系统接口读写测试 */
    /* SD_private_test(void) */

    //写测试
    u8 str[] = "This is a test string.";
    void *fp = eff_fopen("test001.txt", "w+");
    if (!fp) {
        return;
    }
    int wlen = eff_fwrite(fp, str, sizeof(str));
    if (wlen != sizeof(str)) {
        printf("eff_fwrite error\n");
    }
    fclose(fp);

    //读测试
    fp = eff_fopen("test001.txt", "r");
    if (!fp) {
        return;
    }
    char out[64];
    int rlen = eff_fread(fp, out, 64);
    if (rlen) {
        printf("out: %s\n", out);
    }
    fclose(fp);

    printf("================eff verify test ok=================\n");
}

#endif

