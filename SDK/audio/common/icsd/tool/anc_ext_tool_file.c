
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".anc_ext.data.bss")
#pragma data_seg(".anc_ext.data")
#pragma const_seg(".anc_ext.text.const")
#pragma code_seg(".anc_ext.text")
#endif

#include "app_config.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_common.h"
#endif
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
#include "anc_ext_tool.h"
#include "fs/resfile.h"
#include "system/includes.h"
#include "anc.h"

#if 0
#define anc_file_log	printf
#else
#define anc_file_log(...)
#endif

#define ANC_EXT_FILE_DATA_FORMAT_ERR	-1

#define ANC_EXT_HEAD_LEN		20
#define CFG_ANC_EXT_FILE		FLASH_RES_PATH"ALIGN_DIR/anc_ext.bin"
#define ANC_EXT_RSFILE_TAG		"ANCEXT02"
#define GROUP_TYPE_ANC_EXT		0x9841

//anc_ext.bin 文件数据头
struct anc_ext_rsfile_head {
    u32 total_len;	//4 后面所有数据加起来长度
    u16 group_crc;	//6 group_type开始做的CRC，更新数据后需要对应更新CRC
    u16 group_type;	//8
    u16 group_len;  //10 header开始到末尾的长度
    char header[10];//20
};

struct anc_ext_rsfile_hdl {
    u8 *file;		//文件指针
    int file_len;	//文件长度
};

struct anc_ext_rsfile_hdl file_hdl;
static int anc_ext_rsfile_analysis(u8 *file_data, int flen);

//文件数据头校验
static int anc_ext_rsfile_check(struct anc_ext_rsfile_head *head, char *tag)
{
    if (strcmp(head->header, tag) == 0) {
        u16 calc_crc = CRC16(&head->group_type, head->total_len - 2);
        anc_file_log("calc_crc:0x%x", calc_crc);
        if (calc_crc == head->group_crc) {
            anc_file_log("anc_ext file head crc ok\n");
        } else {
            anc_file_log("anc_ext file head crc err\n");
            return -1;
        }
    } else {
        anc_file_log("anc_ext file head tag err\n");
        return -1;
    }
    anc_file_log("%s check succ\n", tag);
    return 0;
}

//FILE_HEAD - file_id1 - file_end - file_id2 - file_end
//anc_ext.bin 文件解析
int anc_ext_rsfile_read(void)
{
    u32 len = 0;
    int ret = 1;
    u8 *file = anc_ext_rsfile_get(&len);
    if (file) {
        ret = anc_ext_rsfile_analysis(file, len);
    }
    return ret;
}

//获取目标文件和长度
u8 *anc_ext_rsfile_get(u32 *file_len)
{
    void *fp = NULL;
    int ret = 0;
    u8 *file;
    struct anc_ext_rsfile_head *f_head;

    fp = resfile_open(CFG_ANC_EXT_FILE);
    if (!fp) {
        printf("[anc_ext.bin] read_faild\n");
        return NULL;
    }
    struct resfile_attrs attr;
    resfile_get_attrs(fp, &attr);
    anc_file_log("faddr:%x,fsize:%d\n", attr.sclust, attr.fsize);
    if (attr.sclust % 4 != 0) {
        /* ASSERT(0, "ERR!!ANC EXT NO ALIGN 4 BYTE\n"); */
        anc_file_log("ERR!!ANC EXT NO ALIGN 4 BYTE\n");
        ret = 1;
        goto __exit;
    }
    file = (u8 *)attr.sclust;
    /* put_buf(file, attr.fsize); */

    f_head = (struct anc_ext_rsfile_head *)file;

    u32 check_total_len = f_head->total_len;
    if ((check_total_len == 0xFFFFFFFF) || \
        (check_total_len == 0) || \
        (f_head->group_type != GROUP_TYPE_ANC_EXT) || \
        (f_head->group_len == 10)) {
        anc_file_log("f_head head err,total_len:%u,group_type:%x,group_len:%u\n", check_total_len, f_head->group_type, f_head->group_len);
        ret = 1;
        goto __exit;
    }

    anc_file_log("total_len:0x%x=%u ", f_head->total_len, f_head->total_len);
    anc_file_log("group_crc:0x%x ", f_head->group_crc);
    anc_file_log("group_type:0x%x ", f_head->group_type);
    anc_file_log("group_len:0x%x=%d ", f_head->group_len, f_head->group_len);
    anc_file_log("tag:%s\n", f_head->header);

    ret = anc_ext_rsfile_check(f_head, ANC_EXT_RSFILE_TAG);

    *file_len = attr.fsize;

__exit:
    if (ret) {
        anc_file_log("anc_ext file get failed\n");
        return NULL;
    }
    anc_file_log("f_head get succ\n");
    resfile_close(fp);
    return file;
}

//解析anc_ext.bin
static int anc_ext_rsfile_analysis(u8 *file_data, int flen)
{
    int ret;
    struct anc_ext_subfile_head *head = NULL;
    //解析文件长度 = anc_ext.bin 长度 - ANC_EXT_HEAD_LEN
    file_data += ANC_EXT_HEAD_LEN;
    flen -= ANC_EXT_HEAD_LEN;
    while (flen) {
        head = (struct anc_ext_subfile_head *)file_data;
        anc_file_log("id:%x , subfile_len:%d\n", head->file_id, head->file_len);
        if (flen < head->file_len) {
            anc_file_log("flen:%d != subfile_len:%d\n", flen, head->file_len);
            ret = ANC_EXT_FILE_DATA_FORMAT_ERR;
            goto __err;
        }
        //根据subfile ID循环解析
        ret = anc_ext_subfile_analysis_each(head->file_id, file_data, head->file_len, 0);
        if (ret) {
            goto __err;
        }
        file_data += head->file_len;
        flen -= head->file_len;
    }
    anc_file_log("%s, suss", __func__);
    return 0;

__err:
    anc_file_log("%s, error! ret = %d", __func__, ret);
    return -EINVAL;

}

// 初始化函数
struct anc_ext_subfile_head  *anc_ext_subfile_catch_init(u32 file_id)
{
    struct anc_ext_subfile_head *head = (struct anc_ext_subfile_head *)anc_malloc("ANC_EXT", sizeof(struct anc_ext_subfile_head));
    if (!head) {
        anc_file_log("Failed to allocate memory");
        return NULL;
    }
    head->file_id = file_id;
    head->file_len = sizeof(struct anc_ext_subfile_head);
    head->version = 0;
    head->id_cnt = 0;
    return head;
}

// 拼接函数
struct anc_ext_subfile_head  *anc_ext_subfile_catch(struct anc_ext_subfile_head *head, u8 *buf, u32 len, u32 id)
{
    struct anc_ext_id_head *new_id_head;
    u32 new_file_len = head->file_len + sizeof(struct anc_ext_id_head) + len;
    // 重新分配内存
    struct anc_ext_subfile_head *new_head = (struct anc_ext_subfile_head *)anc_malloc("ANC_EXT", new_file_len);
    if (new_head) {
        // 复制文件头
        memcpy(new_head, head, head->file_len);
        // 复制数据
        u32 last_id_head_offset = (head->id_cnt) * sizeof(struct anc_ext_id_head);
        u32 new_id_head_offset = (head->id_cnt + 1) * sizeof(struct anc_ext_id_head);
        memcpy(new_head->data + new_id_head_offset, head->data + last_id_head_offset, head->file_len - sizeof(struct anc_ext_subfile_head) - last_id_head_offset);
        anc_free(head);
        head = new_head;
    }
    if (!head) {
        anc_file_log("Failed to reallocate memory");
        return NULL;
    }

    // 更新文件头信息
    head->file_len = new_file_len;
    head->id_cnt += 1;

    // 定位到新ID头的位置
    new_id_head = (struct anc_ext_id_head *)(head->data + (head->id_cnt - 1) * sizeof(struct anc_ext_id_head));
    new_id_head->id = id;

    // 计算新的offset
    // 获取前一个ID头
    u32 data_offset = head->id_cnt * sizeof(struct anc_ext_id_head);
    struct anc_ext_id_head *prev_id_head = (struct anc_ext_id_head *)(head->data + (head->id_cnt - 2) * sizeof(struct anc_ext_id_head));
    new_id_head->offset = ((head->id_cnt == 1) ? 0 : (prev_id_head->offset + prev_id_head->len));
    new_id_head->len = len;

    // 复制数据
    memcpy((u8 *)head->data + data_offset + new_id_head->offset, buf, len);

    return head;
}

//ANC配置文件 使用RES文件区 覆盖ANC_IF预留区的功能
#if 0
#define CFG_ANC_CFG_GAINS_FILE		FLASH_RES_PATH"ALIGN_DIR/anc_gains.bin"
#define CFG_ANC_CFG_COEFF_FILE		FLASH_RES_PATH"ALIGN_DIR/anc_coeff.bin"

#define GROUP_TYPE_ANC_CFG_GAIN		0x9881
#define GROUP_TYPE_ANC_CFG_COEFF	0x98C1


//获取目标文件和长度
u8 *anc_config_rsfile_get(u8 id, u32 *file_len)
{
    void *fp = NULL;
    int ret = 0;
    u8 *file;
    struct anc_ext_rsfile_head *f_head;
    u16 target_type;
    if (id == ANC_DB_COEFF) {
        fp = resfile_open(CFG_ANC_CFG_COEFF_FILE);
        target_type = GROUP_TYPE_ANC_CFG_COEFF;
    } else {
        fp = resfile_open(CFG_ANC_CFG_GAINS_FILE);
        target_type = GROUP_TYPE_ANC_CFG_GAIN;
    }

    if (!fp) {
        printf("[anc_cfg.bin, id = %d], read_faild\n", id);
        return NULL;
    }
    struct resfile_attrs attr;
    resfile_get_attrs(fp, &attr);
    anc_file_log("faddr:%x,fsize:%d\n", attr.sclust, attr.fsize);
    if (attr.sclust % 4 != 0) {
        /* ASSERT(0, "ERR!!ANC EXT NO ALIGN 4 BYTE\n"); */
        printf("ERR!!ANC CFG NO ALIGN 4 BYTE\n");
        ret = 1;
        goto __exit;
    }
    file = (u8 *)attr.sclust;
    /* put_buf(file, attr.fsize); */

    f_head = (struct anc_ext_rsfile_head *)file;

    u32 check_total_len = f_head->total_len;
    if ((check_total_len == 0xFFFFFFFF) || \
        (check_total_len == 0) || \
        (f_head->group_type != target_type) || \
        (f_head->group_len == 10)) {
        anc_file_log("f_head head err,total_len:%u,group_type:%x,group_len:%u\n", check_total_len, f_head->group_type, f_head->group_len);
        ret = 1;
        goto __exit;
    }

    anc_file_log("total_len:0x%x=%u ", f_head->total_len, f_head->total_len);
    anc_file_log("group_crc:0x%x ", f_head->group_crc);
    anc_file_log("group_type:0x%x ", f_head->group_type);
    anc_file_log("group_len:0x%x=%d ", f_head->group_len, f_head->group_len);
    anc_file_log("tag:%s\n", f_head->header);

    *file_len = attr.fsize - ANC_EXT_HEAD_LEN;

__exit:
    if (ret) {
        anc_file_log("anc_ext file get failed\n");
        return NULL;
    }
    anc_file_log("f_head get succ\n");
    resfile_close(fp);
    return (file + ANC_EXT_HEAD_LEN);
}

/*
    对比 资源文件 与 ANC_IF区域 的ANC配置文件，若差异，以资源文件覆盖ANC区域
    查询耗时：2.3ms;  查询并覆盖耗时：41.8ms
*/
int anc_config_rsfile_read(audio_anc_t *param)
{
    u32 rs_gain_len, rs_coeff_len;
    int ret = 0;
    u8 *rs_gain = NULL;
    u8 *rs_coeff = NULL;
    u16 rs_crc, db_crc;
    u8 change_flag = 0;

    u32 last_jiff = jiffies_usec();
    /*对比ANC增益系数*/
    rs_gain = anc_config_rsfile_get(ANC_DB_GAIN, &rs_gain_len);
    if (rs_gain) {
        rs_crc = CRC16(rs_gain, rs_gain_len);
        anc_gain_t *db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &param->gains_size);
        if (db_gain) {
            db_crc = CRC16(db_gain, param->gains_size);
            if (rs_crc != db_crc) {
                change_flag = 1;
                // printf("-----------anc db gains------------");
                // put_buf(db_gain, param->gains_size);
                // printf("-----------anc rsfile gains------------");
                // put_buf(rs_gain, rs_gain_len);
                printf("anc rsfile gains diff, len %d->%d\n", param->gains_size, rs_gain_len);
            }
        }
    } else {
        printf("anc rsfile gains read empty!\n");
    }

    /*对比ANC滤波器系数*/
    rs_coeff = anc_config_rsfile_get(ANC_DB_COEFF, &rs_coeff_len);
    if (rs_coeff) {
        rs_crc = CRC16(rs_coeff, rs_coeff_len);
        anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &param->coeff_size);
        if (db_coeff) {
            db_crc = CRC16(db_coeff, param->coeff_size);
            if (rs_crc != db_crc) {
                change_flag = 1;
                // printf("-----------anc db coeff------------");
                // put_buf(db_coeff, param->coeff_size);
                // printf("-----------anc rsfile coeff------------");
                // put_buf(rs_coeff, rs_coeff_len);
                printf("anc rsfile coeff diff, len %d->%d\n", param->coeff_size, rs_coeff_len);
            }
        }
    } else {
        printf("anc rsfile coeff read empty!\n");
    }

    if (change_flag) {
        param->write_coeff_size = rs_coeff_len;
        ret = anc_db_put(param, rs_gain, rs_coeff);
        if (!ret) {
            printf("anc rsfile change succ\n");
        } else {
            printf("anc rsfile change fail\n");
        }
    }

    printf("anc rsfile check time : %d\n", jiffies_usec2offset(last_jiff, jiffies_usec()));

    return ret;
}
#endif

#endif
