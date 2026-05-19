#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".usb.data.bss")
#pragma data_seg(".usb.data")
#pragma code_seg(".usb.text")
#pragma const_seg(".usb.text.const")
#pragma str_literal_override(".usb.text.const")
#endif

#include "system/includes.h"
#include "usb/device/usb_stack.h"
#include "usb/usb_task.h"
#include "usb/host/hub.h"
#include "usb/usb.h"
#include "usb_config.h"
#include "device/device.h"
#include "system/event.h"
#include "usb_ctrl_transfer.h"
#include "app_config.h"

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[HUB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_HUB_HOST_ENABLE

static u32 _get_hub_descriptor(struct usb_host_device *host_dev, u8 *pBuf)
{
    return usb_control_msg(host_dev,
                           USB_REQ_GET_DESCRIPTOR,
                           USB_DIR_IN | USB_RT_HUB,
                           USB_DT_HUB << 8,
                           0,
                           pBuf,
                           0xff);
    /* 0x47); */
}
static u32 _set_port_featrue(struct usb_host_device *host_dev, u32 port, u32 feature)
{
    return usb_control_msg(host_dev,
                           USB_REQ_SET_FEATURE,
                           USB_RT_PORT,
                           feature,
                           port,
                           NULL,
                           0);
}
static u32 _clear_port_featrue(struct usb_host_device *host_dev, u32 port, u32 feature)
{
    return usb_control_msg(host_dev,
                           USB_REQ_CLEAR_FEATURE,
                           USB_RT_PORT,
                           feature,
                           port,
                           NULL,
                           0);
}
static u32 _get_port_status(struct usb_host_device *host_dev, u32 port, struct usb_port_status *status)
{
    u32 ret = usb_control_msg(host_dev,
                              USB_REQ_GET_STATUS,
                              USB_DIR_IN | USB_RT_PORT,
                              0,
                              port,
                              status,
                              sizeof(struct usb_port_status));
    return ret;
}
static u32 _get_hub_status(struct usb_host_device *host_dev, struct usb_hub_status *status)
{
    u32 ret = usb_control_msg(host_dev,
                              USB_REQ_GET_STATUS,
                              USB_DIR_IN | USB_RT_HUB,
                              0,
                              0,
                              status,
                              sizeof(struct usb_hub_status));
    return ret;
}
static u32 _clear_hub_featrue(struct usb_host_device *host_dev, u32 feature)
{
    return usb_control_msg(host_dev,
                           USB_REQ_CLEAR_FEATURE,
                           USB_RT_HUB,
                           feature,
                           0,
                           NULL,
                           0);
}

struct hub_device_t hub_device[USB_MAX_HW_NUM][1];
static const struct interface_ctrl hub_ctrl = {
    .interface_class = USB_CLASS_HUB,
    .set_power = NULL,//set_power,
    .get_power = NULL,//get_power,
    .ioctl = NULL,
};
static const struct usb_interface_info _usb_if[USB_MAX_HW_NUM][1] = {
    {
        {
            .ctrl = (struct interface_ctrl *) &hub_ctrl,
            .dev.hub = &hub_device[0][0],
        },
        /* { */
        /*     .ctrl = (struct interface_ctrl *) &hub_ctrl, */
        /*     .dev.hub = &hub_device[0][1], */
        /* }, */
        /* { */
        /*     .ctrl = (struct interface_ctrl *) &hub_ctrl, */
        /*     .dev.hub = &hub_device[0][2], */
        /* }, */
        /* { */
        /*     .ctrl = (struct interface_ctrl *) &hub_ctrl, */
        /*     .dev.hub = &hub_device[0][3], */
        /* }, */
    },
#if USB_MAX_HW_NUM > 1
    {
        {
            .ctrl = (struct interface_ctrl *) &hub_ctrl,
            .dev.hub = &hub_device[1][0],
        },
        /* { */
        /*     .ctrl = (struct interface_ctrl *) &hub_ctrl, */
        /*     .dev.hub = &hub_device[1][1], */
        /* }, */
        /* { */
        /*     .ctrl = (struct interface_ctrl *) &hub_ctrl, */
        /*     .dev.hub = &hub_device[1][2], */
        /* }, */
        /* { */
        /*     .ctrl = (struct interface_ctrl *) &hub_ctrl, */
        /*     .dev.hub = &hub_device[1][3], */
        /* }, */
    },
#endif
};

int usb_hub_parser(struct usb_host_device *host_dev, u8 interface_num, const u8 *pBuf)
{
    const u8 *ptr = pBuf;
    log_info("func:%s()\n", __func__);
    const usb_dev usb_id = host_device2id(host_dev);
    const struct usb_interface_info *usb_if = &_usb_if[usb_id][interface_num];
    host_dev->interface_info[interface_num] = usb_if;

    struct usb_interface_descriptor *interface = (struct usb_interface_descriptor *)pBuf;
    pBuf += interface->bLength;
    struct usb_endpoint_descriptor *endpoint = (struct usb_endpoint_descriptor *)pBuf;
    pBuf += endpoint->bLength;

    struct usb_device_descriptor device_desc;
    u32 ret = usb_control_msg(host_dev,
                              USB_REQ_GET_DESCRIPTOR,
                              USB_DIR_IN,
                              (USB_DT_DEVICE << 8),
                              0,
                              &device_desc,
                              USB_DT_DEVICE_SIZE
                             );
    host_dev->private_data.hub_info.protocol = device_desc.bDeviceProtocol;
    if (device_desc.bDeviceProtocol == USB_HUB_PR_HS_MULTI_TT) {
        pBuf += 16; //暂时只作解析处理, 默认使用 STT 模式
        usb_set_interface(host_dev, 0, 1);
    }
    //STT 固定只有一个IN_ep
    usb_if->dev.hub->ep_pair[0] = endpoint->bEndpointAddress;
    usb_if->dev.hub->interval = endpoint->bInterval;

    return (u32)(pBuf - ptr);
}
static void hub_isr(struct usb_host_device *host_dev, u32 ep)
{
    usb_dev usb_id = host_device2id(host_dev);
    u8 buffer[1];
    const struct usb_interface_info *usb_if = host_dev->interface_info[0];
    u32 host_ep = ep;
    u32 target_ep = (usb_if->dev.hub->ep_pair[host_ep]) & 0x7f;
    int rx_len = usb_h_ep_read_async(usb_id, host_ep, target_ep, buffer, sizeof(buffer), USB_ENDPOINT_XFER_INT, 0);
    log_info("hub_isr---hub_status:0x%x\n", buffer[0]);
    if (rx_len < 0) {
        usb_h_ep_read_async(usb_id, host_ep, target_ep, NULL, 0, USB_ENDPOINT_XFER_INT, 1);
    }

    u8 hub_status = buffer[0];
    /* os_taskq_post_msg(USB_TASK_NAME, 4, USBSTACK_HUB_STATUS_MSG, (u32)host_dev, hub_status, host_ep); */
    u32 event = DEVICE_EVENT_CHANGE;
    u32 value = ((hub_status << 16) | host_ep);
    os_taskq_post_msg(USB_TASK_NAME, 4, USBSTACK_HUB_MSG, event, value, (u32)host_dev);

}

static u32 hub_status_changed_process(struct usb_host_device *host_dev)
{
    struct usb_hub_status hub_status;
    _get_hub_status(host_dev, &hub_status);
    while (hub_status.wHubChange) {
        log_debug("hub_status:");
        /* put_buf((u8 *)&hub_status, sizeof(struct usb_hub_status)); */
        if (hub_status.wHubChange & HUB_CHANGE_LOCAL_POWER) {
            _clear_hub_featrue(host_dev, C_HUB_LOCAL_POWER);
        }
        if (hub_status.wHubChange & HUB_CHANGE_OVERCURRENT) {
            _clear_hub_featrue(host_dev, C_HUB_OVER_CURRENT);
        }
        mdelay(10);
        _get_hub_status(host_dev, &hub_status);
    }
    return 0;
}
static int port_status_changed_process(struct usb_host_device *host_dev, u8 port)
{
    u32 ret = 0;
    struct usb_port_status port_status;
    _get_port_status(host_dev, port, &port_status);
    u8 connect_status_change = 0;
    u8 reset_change = 0;
    while (port_status.wPortChange) {
        if (port_status.wPortChange & USB_PORT_STAT_C_CONNECTION) {
            _clear_port_featrue(host_dev, port, USB_PORT_FEAT_C_CONNECTION);
            connect_status_change = 1;
        }
        if (port_status.wPortChange & USB_PORT_STAT_C_ENABLE) {
            _clear_port_featrue(host_dev, port, USB_PORT_FEAT_C_ENABLE);
        }
        if (port_status.wPortChange & USB_PORT_STAT_C_SUSPEND) {
            _clear_port_featrue(host_dev, port, USB_PORT_FEAT_C_SUSPEND);
        }
        if (port_status.wPortChange & USB_PORT_STAT_C_OVERCURRENT) {
            _clear_port_featrue(host_dev, port, USB_PORT_FEAT_C_OVER_CURRENT);
        }
        if (port_status.wPortChange & USB_PORT_STAT_C_RESET) {
            _clear_port_featrue(host_dev, port, USB_PORT_FEAT_C_RESET);
            reset_change = 1;
        }
        mdelay(10);
        _get_port_status(host_dev, port, &port_status);
    }
    u32 event;
    u32 value = port;
    if (connect_status_change) {
        if (port_status.wPortStatus & USB_PORT_STAT_CONNECTION) {
            /* _set_port_featrue(host_dev, port, USB_PORT_FEAT_RESET); */
            event = DEVICE_EVENT_ONLINE;
            os_taskq_post_msg(USB_TASK_NAME, 4, USBSTACK_HUB_MSG, event, value, (u32)host_dev);
        } else {
            event = DEVICE_EVENT_OUT;
            os_taskq_post_msg(USB_TASK_NAME, 4, USBSTACK_HUB_MSG, event, value, (u32)host_dev);
        }
    }
    if (reset_change && (port_status.wPortStatus & USB_PORT_STAT_ENABLE)) {
        event = DEVICE_EVENT_IN;
        if (port_status.wPortStatus & USB_PORT_STAT_HIGH_SPEED) { //是高速设备
            value |= (USB_SPEED_HIGH << 16);
        } else if (port_status.wPortStatus & USB_PORT_STAT_LOW_SPEED) {
            value |= (USB_SPEED_LOW << 16);
        } else {
            value |= (USB_SPEED_FULL << 16);
        }
        ret = -1;
        os_taskq_post_msg(USB_TASK_NAME, 4, USBSTACK_HUB_MSG, event, value, (u32)host_dev);
    }

    return ret;
}
u32 usb_hub_process(u32 usb_id)
{
    int ret;
    log_info("func:%s()\n", __func__);

    struct usb_host_device *host_dev = (struct usb_host_device *)host_id2device(usb_id);

    struct usb_hub_descriptor hub_desc;
    _get_hub_descriptor(host_dev, (u8 *)&hub_desc);

    for (u8 port = 1; port <= hub_desc.bNbrPorts; port++) {
        _set_port_featrue(host_dev, port, USB_PORT_FEAT_POWER);
    }
    mdelay(10);

    struct usb_port_status port_status;
    for (u8 port = 1; port <= hub_desc.bNbrPorts; port++) {
        _get_port_status(host_dev, port, &port_status);
        log_debug("port%d_status:", port);
        /* put_buf((u8 *)&port_status, sizeof(struct usb_port_status)); */
    }

    hub_status_changed_process(host_dev);

    const struct usb_interface_info *usb_if = host_dev->interface_info[0];
    usb_if->dev.hub->nbrports = hub_desc.bNbrPorts;
    u32 target_ep = (usb_if->dev.hub->ep_pair[0]) & 0x7f;
    u32 host_ep = usb_get_ep_num(usb_id, USB_DIR_IN, USB_ENDPOINT_XFER_INT);
    usb_if->dev.hub->ep_pair[host_ep] = target_ep;
    u8 *buffer = (u8 *) & (usb_if->dev.hub->buf);
    usb_h_set_ep_isr(host_dev, host_ep | USB_DIR_IN, hub_isr, (void *)host_dev);
    /* usb_write_rxfuncaddr(usb_id, host_ep, host_dev->private_data.devnum); */
    usb_hub_rxreg_set(usb_id, host_ep, target_ep, &(host_dev->private_data.hub_info));
    usb_h_ep_config(usb_id,  host_ep | USB_DIR_IN, USB_ENDPOINT_XFER_INT, 1,
                    4, buffer, 64); //interval 间隔有疑问,默认先给 4, HS=1ms, FS=4ms
    usb_h_ep_read_async(usb_id, host_ep, target_ep, NULL, 0, USB_ENDPOINT_XFER_INT, 1);
    return 0;
}



u32 usb_hub_event(struct usb_host_device *host_dev, u8 status, u32 host_ep)
{
    int ret = 0;
    if (status == 0) {
        return ret;
    }


    if (status & 0x01) { //hub change detected
        hub_status_changed_process(host_dev);
    }
    //hub change detected
    const struct usb_interface_info *usb_if = host_dev->interface_info[0];
    for (u8 port = 1; port <= usb_if->dev.hub->nbrports; port++) {
        if (status & BIT(port)) {
            ret = port_status_changed_process(host_dev, port);
        }
    }
    usb_dev usb_id = host_device2id(host_dev);
    u32 target_ep = (usb_if->dev.hub->ep_pair[host_ep]) & 0x7f;
    usb_h_ep_read_async(usb_id, host_ep, target_ep, NULL, 0, USB_ENDPOINT_XFER_INT, 1);
    return ret;
}

u32 usb_hub_port_event(struct usb_host_device *host_dev, u32 value, enum hub_port_event port_event)
{
    u8 port = value & 0xFF;
    assert_d(port < 8, "port error\n");
    u8 speed = value >> 16;
    struct usb_host_device *child_dev;
    struct hub_device_t *hub = host_dev->interface_info[0]->dev.hub;
    usb_dev usb_id = host_device2id(host_dev);
    switch (port_event) {
    case USB_HUB_PORT_OUT:
        usb_host_unmount(usb_id, port);
        child_dev = hub->child_dev[port];
        if (child_dev) {
            free(child_dev);
            hub->child_dev[port] = NULL;
        }
        break;
    case USB_HUB_PORT_IN:
        child_dev = malloc(sizeof(struct usb_host_device));
        assert_d((child_dev != NULL), "func:%s(), line:%d\n", __func__, __LINE__);
        log_info("child_dev:0x%x\n", (u32)child_dev);
        memset(child_dev, 0, sizeof(struct usb_host_device));
        hub->child_dev[port] = child_dev;
        child_dev->parent = host_dev;
        child_dev->private_data.hub_info.parent_devnum = host_dev->private_data.devnum;
        child_dev->private_data.hub_info.port = port;
        child_dev->private_data.hub_info.speed = speed;
        mdelay(10);
        usb_host_mount(usb_id,
                       port,
                       TCFG_USB_HOST_MOUNT_RETRY,
                       TCFG_USB_HOST_MOUNT_RESET,
                       TCFG_USB_HOST_MOUNT_TIMEOUT);
        break;
    case USB_HUB_PORT_RESET:
        _set_port_featrue(host_dev, port, USB_PORT_FEAT_RESET);
        break;
    default:
        break;
    }
    return 0;
}





























#endif
