#ifndef JLSTREAM_REPORT_H
#define JLSTREAM_REPORT_H

#include "generic/typedef.h"
#include "generic/list.h"

struct stream_fmt;

struct node_report {
    u16 uuid;
    u8 subid;
    u8 channel;
    u8 bitwide;
    u8 buffered_time;
    u16 sample_rate;
    u16 run_time;
    u16 output_frame;
};

struct node_report_hdl {
    u8 ref;
    u8 free;
    u8 run_time_cnt;
    u32 run_time_sum;
    u32 begin_usec;
    struct list_head entry;
    struct node_report report;
};

struct node_report_hdl *__get_node_report_hdl(u16 uuid, u8 subid);

struct node_report_hdl *get_node_report_hdl(u16 uuid, u8 subid);

void put_node_report_hdl(struct node_report_hdl *hdl);

void __node_report_set_fmt(struct node_report_hdl *hdl, struct stream_fmt *fmt);

const char *jlstream_get_node_report_description();

int jlstream_node_report_create(u16 uuid, u8 subid);

void jlstream_node_report_free(u16 uuid, u8 subid);

void jlstream_node_report_set_fmt(u16 uuid, u8 subid, struct stream_fmt *fmt);

int jlstream_node_report_read(u8 *report, int len, bool *is_end);


#endif

