#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".usb.data.bss")
#pragma data_seg(".usb.data")
#pragma code_seg(".usb.text")
#pragma const_seg(".usb.text.const")
#pragma str_literal_override(".usb.text.const")
#endif

#include "usb/device/usb_stack.h"
#include "usb/usb_config.h"
#include "usb/device/cdc.h"
#include "app_config.h"
#include "os/os_api.h"
#include "cdc_defs.h"  //need redefine __u8, __u16, __u32

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[USB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_USB_SLAVE_CDC_ENABLE

#if TCFG_TUNING_WITH_DUAL_DEVICE_ENBALE
#define CDC_MAX_NUM     2
#else
/*      user_config     */
#define CDC_MAX_NUM     1 //最大 cdc 接口数量
#endif
const static u8 cdc_bulk_ep_in_table[CDC_MAX_NUM] = {
    USB_DIR_IN | CDC_DATA_EP_IN,
#if CDC_MAX_NUM > 1
    USB_DIR_IN | 2,
#endif
};
const static u8 cdc_bulk_ep_out_table[CDC_MAX_NUM] = {
    USB_DIR_OUT | CDC_DATA_EP_OUT,
#if CDC_MAX_NUM > 1
    USB_DIR_OUT | 2,
#endif
};

#if CDC_INTR_EP_ENABLE //默认关闭
const static u8 cdc_intr_ep_in_table[CDC_MAX_NUM] = {
    USB_DIR_IN | CDC_INTR_EP_IN,
#if CDC_MAX_NUM > 1
    USB_DIR_IN | CDC_INTR_EP_IN,
#endif
};
#endif
/*      user_config     */





#if defined(FUSB_MODE) && FUSB_MODE == 0 //high_speed
#define CDC_BULK_MAXP_SIZE  512
/* #define CDC_INTR_INTERVAL   4 */
#else
#define CDC_BULK_MAXP_SIZE  64 //full_speed
/* #define CDC_INTR_INTERVAL   1 */
#endif
#define CDC_INTR_MAXP_SIZE  8
/* #define CDC_INTR_INTERVAL   0xFF */


struct usb_cdc_gadget {
    u8 *buffer;
    u8 *bulk_ep_out_buf;
    u8 *bulk_ep_in_buf;
    /* void *priv; */
    /* int (*output)(void *priv, u8 *obuf, u32 olen); */
    void (*wakeup_handler)(struct usb_device_t *usb_device, u32 num);
    OS_MUTEX mutex_data;

#if CDC_INTR_EP_ENABLE //默认关闭
    OS_MUTEX mutex_intr;
    u8 *intr_ep_in_buffer;
    u8 intr_ep_in;
#endif
    u8 bmTransceiver;
    u8 subtype_data[8];
    u8 comm_feature[2];

    //usb相关信息
    u8 itf_num; //接口号
    u8 bulk_ep_out;
    u8 bulk_ep_in;

    u8 tx_rx_flag;
};

static struct usb_cdc_gadget *cdc_hdl[CDC_MAX_NUM];
#if USB_MALLOC_ENABLE
#else
static struct usb_cdc_gadget _cdc_hdl[CDC_MAX_NUM];
static u8 _cdc_buffer[CDC_MAX_NUM][CDC_BULK_MAXP_SIZE]__attribute__((aligned(4)));
/* static u8 _cdc_dma_out_buf[CDC_MAX_NUM][2 * CDC_BULK_MAXP_SIZE + 8]__attribute__((aligned(4))); */
/* static u8 _cdc_dma_in_buf[CDC_MAX_NUM][CDC_BULK_MAXP_SIZE]__attribute__((aligned(4))); */
#endif


static const u8 cdc_itf_iad[] = {
    // 接口关联描述符 (IAD) [[2]][[9]]
    0x08,           // bLength
    0x0B,           // bDescriptorType (IAD)
    0x00,           // bFirstInterface (从接口0开始)
    0x02,           // bInterfaceCount (2个接口)
    0x02,           // bFunctionClass (CDC类 0x0A) [[5]]
    0x02,           // bFunctionSubClass
    0x01,           // bFunctionProtocol
    0x00,           // iFunction
};
static const u8 cdc_itf_ctrl[] = {
    // 控制接口 (Interface 0) [[1]][[7]]
    0x09,           // bLength
    0x04,           // bDescriptorType (接口描述符)
    0x00,           // bInterfaceNumber
    0x00,           // bAlternateSetting
    0x00,           // bNumEndpoints
    0x02,           // bInterfaceClass (CDC类 0x0A)
    0x02,           // bInterfaceSubClass (Abstract Control Model)
    0x01,           // bInterfaceProtocol
    0x00,           // iInterface
};
static const u8 cdc_head_desc[] = {
    // CDC头功能描述符 [[1]][[7]]
    0x05,           // bLength
    0x24,           // bDescriptorType (CS_INTERFACE)
    0x00,           // bDescriptorSubtype (Header)
    0x10, 0x01,     // bcdCDC (1.10)
};
static const u8 cdc_union_desc[] = {
    // CDC联合功能描述符 [[1]][[7]]
    0x05,           // bLength
    0x24,           // bDescriptorType (CS_INTERFACE)
    0x06,           // bDescriptorSubtype (Union)
    0x00,           // bControlInterface (主接口0)
    0x01,           // bSubordinateInterface0 (从接口1)
};
static const u8 cdc_call_management_desc[] = {
    // CDC呼叫管理功能描述符 [[1]][[7]]
    0x05,           // bLength
    0x24,           // bDescriptorType (CS_INTERFACE)
    0x01,           // bDescriptorSubtype (Call Management)
    0x00,           // bmCapabilities (不支持)
    0x01,           // bDataInterface (数据接口1)
};
static const u8 cdc_intr_ep_desc[] = {
    // 中断端点描述符 (用于控制接口)
    0x07,           // bLength
    0x05,           // bDescriptorType (端点描述符)
    0x81,           // bEndpointAddress (EP1 IN)
    0x03,           // bmAttributes (中断传输)
    0x08, 0x00,     // wMaxPacketSize (8字节)
    0xFF,           // bInterval (255ms)
};
static const u8 cdc_bulk_ep_desc[] = {
    // 数据接口 (Interface 1) [[2]][[5]]
    0x09,           // bLength
    0x04,           // bDescriptorType (接口描述符)
    0x01,           // bInterfaceNumber
    0x00,           // bAlternateSetting
    0x02,           // bNumEndpoints
    0x0A,           // bInterfaceClass (CDC数据类)
    0x00,           // bInterfaceSubClass
    0x00,           // bInterfaceProtocol
    0x00,           // iInterface

    // 批量输出端点 (EP2 OUT)
    0x07,           // bLength
    0x05,           // bDescriptorType (端点描述符)
    0x02,           // bEndpointAddress (EP2 OUT)
    0x02,           // bmAttributes (批量传输)
    0x40, 0x00,     // wMaxPacketSize (64字节)
    0x00,           // bInterval

    // 批量输入端点 (EP3 IN)
    0x07,           // bLength
    0x05,           // bDescriptorType (端点描述符)
    0x83,           // bEndpointAddress (EP3 IN)
    0x02,           // bmAttributes (批量传输)
    0x40, 0x00,     // wMaxPacketSize (64字节)
    0x00            // bInterval
};


static u32 cdc_itf2num(u32 itf)
{
    for (u8 i = 0; i < CDC_MAX_NUM; i++) {
        if (cdc_hdl[i]->itf_num == itf) {
            return i;
        }
    }
    ASSERT(0, "itf to num fail  itf:%d", itf);
    return 0;
}
static u32 cdc_ep2num(u32 ep)
{
    for (u8 i = 0; i < CDC_MAX_NUM; i++) {
        if ((cdc_hdl[i]->bulk_ep_out) == ep) {
            return i;
        }
    }
    for (u8 i = 0; i < CDC_MAX_NUM; i++) {
        if ((cdc_hdl[i]->bulk_ep_in) == ep) {
            return i;
        }
    }
    ASSERT(0, "ep to num fail  ep:%d", ep);
    return 0;
}
void cdc_set_wakeup_handler(u32 num, void (*handle)(struct usb_device_t *usb_device, u32 num))
{
    if (num >= CDC_MAX_NUM) {
        return;
    }
    /*ASSERT(num < CDC_MAX_NUM, "cdc_set_wakeup_handler num:%d > CDC_MAX_NUM:%d", num, CDC_MAX_NUM);*/
    if (cdc_hdl[num]) {
        cdc_hdl[num]->wakeup_handler = handle;
    }
}

static void cdc_bulk_rx(struct usb_device_t *usb_device, u32 ep)
{
    u8 num = cdc_ep2num(ep);
    if (cdc_hdl[num] && cdc_hdl[num]->wakeup_handler) {
        cdc_hdl[num]->wakeup_handler(usb_device, num);
    } else {
        const usb_dev usb_id = usb_device2id(usb_device);
        u8 *cdc_rx_buf = cdc_hdl[num]->buffer;
        u32 len = usb_g_bulk_read(usb_id, ep, cdc_rx_buf, CDC_BULK_MAXP_SIZE, 0);
    }
}
static void cdc_bulk_tx(struct usb_device_t *usb_device, u32 ep)
{
}
u32 cdc_read_data(const usb_dev usb_id, u8 *buf, u32 len, u32 num)
{
    if (cdc_hdl[num] == NULL) {
        return 0;
    }
    if (cdc_hdl[num]->tx_rx_flag == 0) {
        return 0;
    }
    u8 *cdc_rx_buf = cdc_hdl[num]->buffer;
    u32 ep = cdc_hdl[num]->bulk_ep_out & 0x7f;
    os_mutex_pend(&cdc_hdl[num]->mutex_data, 0);
    //由于bulk传输使用双缓冲，无法用usb_get_ep_buffer()知道是哪一个buffer，需要外部buffer接收数据
    u32 rxlen = usb_g_bulk_read(usb_id, ep, cdc_rx_buf, CDC_BULK_MAXP_SIZE, 0);
    rxlen = rxlen > len ? len : rxlen;
    if ((rxlen > 0) && buf) {
        memcpy(buf, cdc_rx_buf, rxlen);
    }
    os_mutex_post(&cdc_hdl[num]->mutex_data);
    return rxlen;
}
u32 cdc_write_data(const usb_dev usb_id, u8 *buf, u32 len, u32 num)
{
    u32 txlen, offset;
    if (cdc_hdl[num] == NULL) {
        return 0;
    }
    if (cdc_hdl[num]->tx_rx_flag == 0) {
        return 0;
    }
    if ((cdc_hdl[num]->bmTransceiver & (BIT(1) | BIT(4))) != (BIT(1) | BIT(4))) {
        return 0;
    }
    if (!(cdc_hdl[num]->bmTransceiver & BIT(0))) {
        return 0;
    }
    u32 ep = cdc_hdl[num]->bulk_ep_in & 0x7f;
    offset = 0;
    os_mutex_pend(&cdc_hdl[num]->mutex_data, 0);
    while (offset < len) {
        txlen = len - offset > MAXP_SIZE_CDC_BULKIN ?
                MAXP_SIZE_CDC_BULKIN : len - offset;
        txlen = usb_g_bulk_write(usb_id, ep, buf + offset, txlen);
        if (txlen == 0) {
            break;
        }
        if ((cdc_hdl[num]->bmTransceiver & (BIT(1) | BIT(4))) != (BIT(1) | BIT(4))) {
            break;
        }
        offset += txlen;
    }
    //当最后一包的包长等于maxpktsize，需要发一个0长包表示结束
    if ((offset % MAXP_SIZE_CDC_BULKIN) == 0) {
        usb_g_bulk_write(usb_id, ep, NULL, 0);
    }
    os_mutex_post(&cdc_hdl[num]->mutex_data);
    return offset;
}

static void cdc_endpoint_init(struct usb_device_t *usb_device, u32 itf)
{
    u8 num = cdc_itf2num(itf);

    ASSERT(cdc_hdl[num], "cdc not register");
    cdc_hdl[num]->tx_rx_flag = 0;
    const usb_dev usb_id = usb_device2id(usb_device);


    usb_g_ep_config(usb_id, cdc_hdl[num]->bulk_ep_in, USB_ENDPOINT_XFER_BULK,
                    0, cdc_hdl[num]->bulk_ep_in_buf, CDC_BULK_MAXP_SIZE);
    usb_g_ep_config(usb_id, cdc_hdl[num]->bulk_ep_out, USB_ENDPOINT_XFER_BULK,
                    1, cdc_hdl[num]->bulk_ep_out_buf, CDC_BULK_MAXP_SIZE);
    usb_g_set_intr_hander(usb_id, cdc_hdl[num]->bulk_ep_out, cdc_bulk_rx);
    usb_enable_ep(usb_id, cdc_hdl[num]->bulk_ep_out);

    /* usb_g_set_intr_hander(usb_id, cdc_hdl[num]->bulk_ep_out, cdc_bulk_tx); */
    /* usb_enable_ep(usb_id, cdc_hdl[num]->bulk_ep_in & 0x7f); */
}
static void cdc_reset(struct usb_device_t *usb_device, u32 itf)
{
    log_debug("%s", __func__);
    const usb_dev usb_id = usb_device2id(usb_device);
    cdc_endpoint_init(usb_device, itf);
}
static void dump_line_coding(struct usb_cdc_line_coding *lc)
{
    log_info("dtw rate      : %d", lc->dwDTERate);
    log_info("stop bits     : %d", lc->bCharFormat);
    log_info("verify bits   : %d", lc->bParityType);
    log_info("data bits     : %d", lc->bDataBits);
}
static u32 cdc_setup_rx(struct usb_device_t *usb_device, struct usb_ctrlrequest *ctrl_req)
{
    u8 num = cdc_itf2num(LOBYTE(ctrl_req->wIndex));
    const usb_dev usb_id = usb_device2id(usb_device);
    int recip_type;
    struct usb_cdc_line_coding *lc = 0;
    u32 len;
    u8 *read_ep = usb_get_setup_buffer(usb_device);

    len = ctrl_req->wLength;
    usb_read_ep0(usb_id, read_ep, len);
    recip_type = ctrl_req->bRequestType & USB_TYPE_MASK;
    switch (recip_type) {
    case USB_TYPE_CLASS:
        switch (ctrl_req->bRequest) {
        case USB_CDC_SEND_ENCAPSULATED_COMMAND:
            log_info("USB_CDC_SEND_ENCAPSULATED_COMMAND");
            log_debug_hexdump(read_ep, len);
            break;
        case 0x02:  //Set_Comm_Feature
            log_info("USB_CDC_REQ_SET_COMM_FEATURE");
            if (cdc_hdl[num] == NULL) {
                break;
            }
            memcpy(cdc_hdl[num]->comm_feature, read_ep, len);
            log_debug_hexdump(read_ep, len);
            break;
        case USB_CDC_REQ_SET_LINE_CODING:
            log_info("USB_CDC_REQ_SET_LINE_CODING");
            if (cdc_hdl[num] == NULL) {
                break;
            }
            if (len > sizeof(struct usb_cdc_line_coding)) {
                len = sizeof(struct usb_cdc_line_coding);
            }
            memcpy(cdc_hdl[num]->subtype_data, read_ep, len);
            lc = (struct usb_cdc_line_coding *)cdc_hdl[num]->subtype_data;
            dump_line_coding(lc);
            break;
        }
        break;
    }
    return USB_EP0_STAGE_SETUP;
}
static u32 cdc_setup(struct usb_device_t *usb_device, struct usb_ctrlrequest *ctrl_req)
{
    u8 num = cdc_itf2num(LOBYTE(ctrl_req->wIndex));
    const usb_dev usb_id = usb_device2id(usb_device);
    int recip_type;
    u32 len;

    recip_type = ctrl_req->bRequestType & USB_TYPE_MASK;

    switch (recip_type) {
    case USB_TYPE_CLASS:
        switch (ctrl_req->bRequest) {
        case USB_CDC_SEND_ENCAPSULATED_COMMAND:
            usb_set_setup_recv(usb_device, cdc_setup_rx);
            break;
        case USB_CDC_GET_ENCAPSULATED_RESPONSE:
            //Not need to transfer ITU-T V250 AT command,
            //just sample for this command
            memset(usb_get_setup_buffer(usb_device), 0, ctrl_req->wLength);
            usb_set_data_payload(usb_device, ctrl_req, usb_get_setup_buffer(usb_device), ctrl_req->wLength);
            break;
        case 0x02:  //Set_Comm_Feature
            usb_set_setup_recv(usb_device, cdc_setup_rx);
            break;
        case 0x03:  //Clear_Comm_Feature
            if (ctrl_req->wValue == 0x01) {  //ABSTRACT_STATE
                cdc_hdl[num]->comm_feature[0] = 0;
                usb_set_setup_phase(usb_device, USB_EP0_STAGE_SETUP);
            }
            break;
        case 0x04:  //Get_Comm_Feature
            if (ctrl_req->wValue == 0x01) {  //ABSTRACT_STATE
                usb_set_data_payload(usb_device, ctrl_req, cdc_hdl[num]->comm_feature, ctrl_req->wLength);
            }
            break;
        case USB_CDC_REQ_SET_LINE_CODING:
            log_info("set line coding");
            usb_set_setup_recv(usb_device, cdc_setup_rx);
            break;
        case USB_CDC_REQ_GET_LINE_CODING:
            log_info("get line codling");
            len = ctrl_req->wLength < sizeof(struct usb_cdc_line_coding) ?
                  ctrl_req->wLength : sizeof(struct usb_cdc_line_coding);
            if (cdc_hdl[num] == NULL) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
                break;
            }
            usb_set_data_payload(usb_device, ctrl_req, cdc_hdl[num]->subtype_data, len);
            dump_line_coding((struct usb_cdc_line_coding *)cdc_hdl[num]->subtype_data);
            cdc_hdl[num]->tx_rx_flag = 1;
            break;
        case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
            log_info("set control line state - %d", ctrl_req->wValue);
            if (cdc_hdl[num]) {
                /* if (ctrl_req->wValue & BIT(0)) { //DTR */
                cdc_hdl[num]->bmTransceiver |= BIT(0);
                /* } else { */
                /* cdc_hdl[num]->bmTransceiver &= ~BIT(0); */
                /* } */
                /* if (ctrl_req->wValue & BIT(1)) { //RTS */
                cdc_hdl[num]->bmTransceiver |= BIT(1);
                /* } else { */
                /* usb_slave->cdc->bmTransceiver &= ~BIT(1); */
                /* } */
                cdc_hdl[num]->bmTransceiver |= BIT(4);  //cfg done
            }
            usb_set_setup_phase(usb_device, USB_EP0_STAGE_SETUP);
            //cdc_endpoint_init(usb_device, (ctrl_req->wIndex & USB_RECIP_MASK));
            break;
        default:
            log_error("unsupported class req");
            usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            break;
        }
        break;
    default:
        log_error("unsupported req type");
        usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
        break;
    }
    return 0;
}


u32 cdc_desc_config(const usb_dev usb_id, u8 *ptr, u32 *itf)
{
    struct usb_device_t *usb_device = usb_id2device(usb_id);
    u8 *tptr = ptr;

    for (u8 num = 0; num < CDC_MAX_NUM; num++) {
        memcpy(tptr, (u8 *)cdc_itf_iad, sizeof(cdc_itf_iad));
        tptr[2] = *itf; //从 itf接口开始
        tptr[3] = 2; //接口数量
        tptr += sizeof(cdc_itf_iad);

        memcpy(tptr, (u8 *)cdc_itf_ctrl, sizeof(cdc_itf_ctrl));
        tptr[2] = *itf;
#if CDC_INTR_EP_ENABLE //默认关闭
        tptr[4] = 1;
#endif
        tptr += sizeof(cdc_itf_ctrl);

        memcpy(tptr, (u8 *)cdc_head_desc, sizeof(cdc_head_desc));
        tptr += sizeof(cdc_head_desc);

        memcpy(tptr, (u8 *)cdc_union_desc, sizeof(cdc_union_desc));
        tptr[3] = *itf;
        tptr[4] = *itf + 1;
        tptr += sizeof(cdc_union_desc);

        memcpy(tptr, (u8 *)cdc_call_management_desc, sizeof(cdc_call_management_desc));
        tptr[4] = *itf + 1;
        tptr += sizeof(cdc_call_management_desc);

#if CDC_INTR_EP_ENABLE
        memcpy(tptr, (u8 *)cdc_intr_ep_desc, sizeof(cdc_intr_ep_desc));
        tptr[2] = cdc_intr_ep_in_table[num];
        tptr[4] = LOBYTE(CDC_INTR_MAXP_SIZE);
        tptr[5] = HIBYTE(CDC_INTR_MAXP_SIZE);
        tptr[6] = 0xFF;//CDC_INTR_INTERVAL;
        tptr += sizeof(cdc_intr_ep_desc);
#endif

        memcpy(tptr, (u8 *)cdc_bulk_ep_desc, sizeof(cdc_bulk_ep_desc));
        tptr[2] = *itf + 1;
        tptr[9 + 2] = cdc_bulk_ep_out_table[num];
        tptr[9 + 4] = LOBYTE(CDC_BULK_MAXP_SIZE);
        tptr[9 + 5] = HIBYTE(CDC_BULK_MAXP_SIZE);
        tptr[9 + 7 + 2] = cdc_bulk_ep_in_table[num];
        tptr[9 + 7 + 4] = LOBYTE(CDC_BULK_MAXP_SIZE);
        tptr[9 + 7 + 5] = HIBYTE(CDC_BULK_MAXP_SIZE);
        tptr += sizeof(cdc_bulk_ep_desc);

        log_debug("cdc%d_ctrl interface num:%d\n", num, *itf);
        if (usb_set_interface_hander(usb_id, *itf, cdc_setup) != *itf) {
            ASSERT(0, "cdc set interface_hander fail");
        }
        if (usb_set_reset_hander(usb_id, *itf, cdc_reset) != *itf) {
            ASSERT(0, "cdc set reset_hander fail");
        }
        log_debug("cdc%d_data interface num:%d\n", num, *itf + 1);

        cdc_hdl[num]->itf_num = *itf;
#if CDC_INTR_EP_ENABLE
        cdc_hdl[num]->intr_ep_in = cdc_intr_ep_in_table[num];
#endif
        cdc_hdl[num]->bulk_ep_out = cdc_bulk_ep_out_table[num];
        cdc_hdl[num]->bulk_ep_in = cdc_bulk_ep_in_table[num];
        log_info("cdc_hdl[%d] add success \n", num);

        *itf += 2;
    }

    return (u32)(tptr - ptr);
}

static void usb_cdc_line_coding_init(struct usb_cdc_line_coding *lc)
{
    lc->dwDTERate = 1000000;//460800;
    lc->bCharFormat = USB_CDC_1_STOP_BITS;
    lc->bParityType = USB_CDC_NO_PARITY;
    lc->bDataBits = 8;
}

void cdc_register(const usb_dev usb_id)
{
    struct usb_cdc_line_coding *lc;
    for (u8 num = 0; num < CDC_MAX_NUM; num++) {
        if (cdc_hdl[num] != NULL) {
            continue;
        }
#if USB_MALLOC_ENABLE
        cdc_hdl[num] = zalloc(sizeof(struct usb_cdc_gadget));
        if (!cdc_hdl[num]) {
            log_error("cdc_hdl[%d] register err\n", num);
            return;
        }
        cdc_hdl[num]->buffer = malloc(CDC_BULK_MAXP_SIZE);
        if (!cdc_hdl[num]->buffer) {
            log_error("cdc_hdl[%d]->buffer register err\n", num);
            free(cdc_hdl[num]);
            cdc_hdl[num] = NULL;
            return;
        }
#else
        memset(&_cdc_hdl[num], 0, sizeof(struct usb_cdc_gadget));
        cdc_hdl[num] = &_cdc_hdl[num];
        cdc_hdl[num]->buffer = _cdc_buffer[num];
#endif
        lc = (struct usb_cdc_line_coding *)cdc_hdl[num]->subtype_data;
        usb_cdc_line_coding_init(lc);
        os_mutex_create(&(cdc_hdl[num]->mutex_data));

        cdc_hdl[num]->bulk_ep_out_buf = usb_alloc_ep_dmabuffer(usb_id, cdc_bulk_ep_out_table[num], CDC_BULK_MAXP_SIZE * 2);
        cdc_hdl[num]->bulk_ep_in_buf = usb_alloc_ep_dmabuffer(usb_id, cdc_bulk_ep_in_table[num], CDC_BULK_MAXP_SIZE);

#if CDC_INTR_EP_ENABLE
        os_mutex_create(&(cdc_hdl[num]->mutex_intr));
        cdc_hdl[num]->intr_ep_in_buf = usb_alloc_ep_dmabuffer(usb_id, cdc_intr_ep_in_table[num], CDC_INTR_MAXP_SIZE);
#endif

        log_info("cdc%d_buffer:0x%x\n", num, cdc_hdl[num]->buffer);
        log_info("cdc%d_bulk_ep_out_buf:0x%x\n", num, cdc_hdl[num]->bulk_ep_out_buf);
        log_info("cdc%d_bulk_ep_in_buf:0x%x\n", num, cdc_hdl[num]->bulk_ep_in_buf);
    }
}

void cdc_release(const usb_dev usb_id)
{
    for (u8 num = 0; num < CDC_MAX_NUM; num++) {
        if (cdc_hdl[num] == NULL) {
            continue;
        }
        cdc_hdl[num]->tx_rx_flag = 0;
        if (cdc_hdl[num]->bulk_ep_out_buf) {
            usb_free_ep_dmabuffer(usb_id, cdc_hdl[num]->bulk_ep_out_buf);
            cdc_hdl[num]->bulk_ep_out_buf = NULL;
        }
        if (cdc_hdl[num]->bulk_ep_in_buf) {
            usb_free_ep_dmabuffer(usb_id, cdc_hdl[num]->bulk_ep_in_buf);
            cdc_hdl[num]->bulk_ep_in_buf = NULL;
        }
#if CDC_INTR_EP_ENABLE
        if (cdc_hdl[num]->intr_ep_in_buf) {
            usb_free_ep_dmabuffer(usb_id, cdc_hdl[num]->intr_ep_in_buf);
            cdc_hdl[num]->intr_ep_in_buf = NULL;
        }
#endif
#if USB_MALLOC_ENABLE
        free(cdc_hdl[num]->buffer);
        free(cdc_hdl[num]);
#endif
        cdc_hdl[num] = NULL;
    }
}

#endif
