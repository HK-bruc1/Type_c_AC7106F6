#ifndef  __HUB_H__
#define  __HUB_H__

#include "usb/host/usb_host.h"
#include "usb/host/ch11.h"

enum hub_port_event {
    USB_HUB_PORT_OUT = 0,
    USB_HUB_PORT_IN,
    USB_HUB_PORT_RESET,
};

struct hub_device_t {
    void *parent; ///<定义的parent指针，指向该hid的所属设备
    u8 ep_pair[6]; ///<端点对数组，数组下标存放主机端点，对应的元素存放目标端点
    u8 interval;
    u32 buf; //hub常见就一个byte
    u8 nbrports;
    struct usb_host_device *child_dev[7];
    // u8 report_count; ///<报告描述符中item计数器
    // u8 bNumEndpoints; ///<端点数量
    // struct report_info_t report_list[MAX_REPORT_COUNT]; ///<报告描述符结构体
};

int usb_hub_parser(struct usb_host_device *host_dev, u8 interface_num, const u8 *pBuf);
u32 usb_hub_process(u32 usb_id);
u32 usb_hub_event(struct usb_host_device *host_dev, u8 hub_status, u32 host_ep);
u32 usb_hub_port_reset_event(struct usb_host_device *host_dev, u32 value);
u32 usb_hub_port_event(struct usb_host_device *host_dev, u32 value, enum hub_port_event port_event);



#endif  /*HUB_H*/

