#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".usb.data.bss")
#pragma data_seg(".usb.data")
#pragma code_seg(".usb.text")
#pragma const_seg(".usb.text.const")
#pragma str_literal_override(".usb.text.const")
#endif

#include "system/includes.h"
#include "usb/device/usb_stack.h"
#include "usb/device/mtp.h"
#include "usb/scsi.h"
#include "usb_config.h"
#include "device/device.h"
#include "app_config.h"
#include "file_operate/file_manager.h"

#if TCFG_DEV_MANAGER_ENABLE
#include "dev_manager.h"
#endif
#if TCFG_USB_SLAVE_MTP_ENABLE

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[USB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

static const u8 sMtpDescriptor[] = {  //<Interface & Endpoint Descriptor
    // 接口描述符
    9,            // bLength
    4,            // bDescriptorType (INTERFACE)
    0,            // bInterfaceNumber
    0,            // bAlternateSetting
    3,            // bNumEndpoints
    0x06,         // bInterfaceClass (通信类)
    0x01,         // bInterfaceSubClass
    0x01,         // bInterfaceProtocol (MTP)
    0,            // iInterface

    // 端点描述符（IN）
    7,            // bLength
    5,            // bDescriptorType (ENDPOINT)
    USB_DIR_IN | MTP_BULK_EP_IN,
    0x02,         // bmAttributes (同步)
    LOBYTE(MAXP_SIZE_BULKIN), HIBYTE(MAXP_SIZE_BULKIN),
    0,            // bInterval

    // 端点描述符（OUT）
    7,            // bLength
    5,            // bDescriptorType (ENDPOINT)
    USB_DIR_OUT | MTP_BULK_EP_OUT,
    0x02,         // bmAttributes (同步)
    LOBYTE(MAXP_SIZE_BULKOUT), HIBYTE(MAXP_SIZE_BULKOUT),
    0,            // bInterval

    //EndpointDescriptor:
    USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType, Type
    USB_DIR_IN | MTP_INTR_EP_IN,// bEndpointAddress
    USB_ENDPOINT_XFER_INT,      // Interrupt
    LOBYTE(64), HIBYTE(64),// Maximum packet size
    6,          // bInterval, for high speed 2^(n-1) * 125us, for full/low speed n * 1ms */
};
// 定义 GetDeviceInfo 返回的结构体
const static u16 DeviceInfoStandardVersion = 100; //表示1.00
const static u32 DeviceInfoVendorExtensionID = 0x00000006;
const static u16 DeviceInfoVendorExtensionVersion = 100; //表示1.00
const static u8 DeviceInfoVendorExtensionDesc[] = {
    'J', 0x00,
    'i', 0x00,
    'e', 0x00,
    'L', 0x00,
    'i', 0x00,
    'T', 0x00,
    'e', 0x00,
    'c', 0x00,
    'h', 0x00,
    'n', 0x00,
    'o', 0x00,
    'l', 0x00,
    'o', 0x00,
    'g', 0x00,
    'y', 0x00,
};
const static u16 DeviceInfoFunctionalMode = 0x0000;
const static u16 DeviceInfoOperationsSupported[] = {
    MTP_OPERATION_GET_DEVICE_INFO,
    MTP_OPERATION_OPEN_SESSION,
    MTP_OPERATION_CLOSE_SESSION,
    MTP_OPERATION_GET_STORAGE_IDS,
    MTP_OPERATION_GET_STORAGE_INFO,
    MTP_OPERATION_GET_NUM_OBJECTS,
    MTP_OPERATION_GET_OBJECT_HANDLES,
    MTP_OPERATION_GET_OBJECT_INFO,
    MTP_OPERATION_GET_OBJECT,
    MTP_OPERATION_GET_THUMB,
    MTP_OPERATION_DELETE_OBJECT,
    MTP_OPERATION_SEND_OBJECT_INFO,
    MTP_OPERATION_SEND_OBJECT,
};
const static u16 DeviceInfoEventsSupported[] = {
    MTP_EVENT_OBJECT_ADDED,
    MTP_EVENT_OBJECT_REMOVED,
    MTP_EVENT_STORE_ADDED,
    MTP_EVENT_STORE_REMOVED,
    MTP_EVENT_DEVICE_PROP_CHANGED,
    MTP_EVENT_OBJECT_INFO_CHANGED,
    MTP_EVENT_STORAGE_INFO_CHANGED,
};
const static u16 DeviceInfoDevicePropertiesSupported[] = {
    MTP_DEVICE_PROPERTY_SYNCHRONIZATION_PARTNER,
    MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME,
    MTP_DEVICE_PROPERTY_IMAGE_SIZE,
    MTP_DEVICE_PROPERTY_BATTERY_LEVEL,
    MTP_DEVICE_PROPERTY_PERCEIVED_DEVICE_TYPE,
};
const static u16 DeviceInfoDeviceCaptureFormats[] = {
};
const static u16 DeviceInfoDeviceImageFormats[] = {
    MTP_FORMAT_MP3,
};
const static u8 DeviceInfoDeviceManufacturer[] = {
    'J', 0x00,
    'i', 0x00,
    'e', 0x00,
    'L', 0x00,
    'i', 0x00,
    0x00, 0x00,
};
const static u8 DeviceInfoModel[] = {
    'B', 0x00,
    'R', 0x00,
    '2', 0x00,
    '9', 0x00,
    '_', 0x00,
    'M', 0x00,
    'T', 0x00,
    'P', 0x00,
    0x00, 0x00,
};
const static u8 DeviceInfoDeviceVersion[] = {
    '1', 0x00,
    '.', 0x00,
    '0', 0x00,
    '0', 0x00,
    0x00, 0x00,
};
const static u8 DeviceInfoDeviceSerialNumber[] = {
    0xA4, 0x7C,
    'A', 0x00,
    'G', 0x00,
    '2', 0x00,
    '1', 0x00,
    '9', 0x00,
    '2', 0x00,
    '6', 0x00,
    0x00, 0x00,
};

struct usb_mtp_handle *mtp_handle;
#if USB_MALLOC_ENABLE
#else
#if USB_MTP_BULK_DEV_USE_ASYNC
static u8 mtp_buf[MTP_BUFFER_SIZE * 2]  SEC(.usb.data.bss.exchange) __attribute__((aligned(64)));
#else
static u8 mtp_buf[MTP_BUFFER_SIZE] SEC(.usb.data.bss.exchange) __attribute__((aligned(64)));
#endif
static struct usb_mtp_handle _mtp_handle SEC(.usb.data.bss.exchange);
#endif

static void mtp_command_block_dump()
{
    printf("mtp command block:\n");
    printf("ContainerLength:0x%x\n", mtp_handle->command.container_length);
    printf("ContainerType:0x%x\n", mtp_handle->command.container_type);
    printf("Code:0x%x\n", mtp_handle->command.code);
    printf("TransactionID:0x%x\n", mtp_handle->command.transaction_id);
    /* printf("Parameter1:0x%x\n", mtp_handle->command.parameter1); */
    /* printf("Parameter2:0x%x\n", mtp_handle->command.parameter2); */
    /* printf("Parameter3:0x%x\n", mtp_handle->command.parameter3); */
    /* printf("Parameter4:0x%x\n", mtp_handle->command.parameter4); */
    /* printf("Parameter5:0x%x\n", mtp_handle->command.parameter5); */
}
static void mtp_response_block_dump()
{
    printf("mtp_response block:\n");
    printf("ContainerLength:0x%x\n", mtp_handle->response.container_length);
    printf("ContainerType:0x%x\n", mtp_handle->response.container_type);
    printf("Cod:0x%x\n", mtp_handle->response.code);
    printf("TransactionID:0x%x\n", mtp_handle->response.transaction_id);
    /* printf("Parameter1:0x%x\n", mtp_handle->response.parameter1); */
    /* printf("Parameter2:0x%x\n", mtp_handle->response.parameter2); */
    /* printf("Parameter3:0x%x\n", mtp_handle->response.parameter3); */
    /* printf("Parameter4:0x%x\n", mtp_handle->response.parameter4); */
    /* printf("Parameter5:0x%x\n", mtp_handle->response.parameter5); */
}
void mtp_dump()
{
    mtp_command_block_dump();
    mtp_response_block_dump();
}

static u32 mtp_block_fill(struct mtp_block_header *header, u16 container_type, u16 code, u32 transaction_id, const u8 *data, u32 data_len);
static void mtp_set_state(struct usb_device_t *usb_device, enum mtp_state state);
static u32 mtp_recv_cancel_request(struct usb_device_t *usb_device, struct usb_ctrlrequest *setup)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    const usb_dev usb_id = usb_device2id(usb_device);
    u32 ret = 0;
    u8 read_ep[EP0_SETUP_LEN];
    usb_read_ep0(usb_id, read_ep, MIN(sizeof(read_ep), setup->wLength));
    ret = USB_EP0_STAGE_SETUP;

    u16 event_code;
    memcpy(&event_code, &read_ep[0], 2);
    u32 transid;
    memcpy(&transid, &read_ep[2], 4);

    if (event_code == MTP_EVENT_CANCEL_TRANSACTION) {
        /* usb_clr_intr_txe(usb_id, MTP_BULK_EP_IN); */
        struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->response;
        mtp_block_fill(header, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESPONSE_OK, mtp_handle->data.transaction_id, NULL, 0);
        mtp_set_state(usb_device, MTP_STATE_RESPONSE);
    }
    return ret;
}
static void mtp_req_get_device_status(struct usb_ctrlrequest *req, u8 *buf, u32 *len)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 offset = 0;

    u16 length = req->wLength;
    memcpy(&buf[offset], &length, sizeof(length));
    offset += sizeof(length);

    u16 code = MTP_RESPONSE_OK;
    memcpy(&buf[offset], &code, sizeof(code));
    offset += sizeof(code);

    u32 transid = mtp_handle->command.transaction_id;
    memcpy(&buf[offset], &transid, sizeof(transid));
    offset += sizeof(transid);

    u8 status_desc[28] = {0};
    memcpy(&buf[offset], status_desc, sizeof(status_desc));
    offset += sizeof(status_desc);

    *len = offset;

    put_buf(buf, *len);
}
static void mtp_reset(struct usb_device_t *usb_device, u32 itf_num);
static u32 mtp_itf_hander(struct usb_device_t *usb_device, struct usb_ctrlrequest *req)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u32 tx_len;
    u8 *tx_payload = usb_get_setup_buffer(usb_device);
    u32 bRequestType = req->bRequestType & USB_TYPE_MASK;
    switch (bRequestType) {
    case USB_TYPE_CLASS:
        switch (req->bRequest) {
        case MTP_REQ_CANCEL:
            usb_set_setup_recv(usb_device, mtp_recv_cancel_request);
            break;
        case MTP_REQ_GET_EXT_EVENT_DATA:
            break;
        case MTP_REQ_RESET:
            /* usb_clr_intr_txe(usb_id, MTP_BULK_EP_IN); */
            mtp_reset(usb_device, 0);
            break;
        case MTP_REQ_GET_DEVICE_STATUS:
            mtp_req_get_device_status(req, tx_payload, &tx_len);
            usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return 0;
}
u32 mtp_set_wakeup_handle(void (*handle)(struct usb_device_t *usb_device))
{
    if (mtp_handle) {
        mtp_handle->info.mtp_wakeup_handle = handle;
        return 1;
    }
    return 0;
}
static void mtp_wakeup(struct usb_device_t *usb_device, u32 ep)
{
    if ((mtp_handle) && (mtp_handle->info.mtp_wakeup_handle)) {
        mtp_handle->info.mtp_wakeup_handle(usb_device);
    }
}
static void mtp_endpoint_init(struct usb_device_t *usb_device, u32 itf)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u32 maxp_size_bulkin = MAXP_SIZE_BULKIN;
    u32 maxp_size_bulkout = MAXP_SIZE_BULKOUT;

#if defined(FUSB_MODE) && FUSB_MODE == 0
    if (usb_device->bSpeed == USB_SPEED_FULL) {
        maxp_size_bulkin = MAXP_SIZE_BULKIN_FS;
        maxp_size_bulkout = MAXP_SIZE_BULKOUT_FS;
    }
#endif
    usb_g_ep_config(usb_id, MTP_BULK_EP_IN | USB_DIR_IN, USB_ENDPOINT_XFER_BULK, 0, mtp_handle->ep_out_dmabuffer, maxp_size_bulkin);

    usb_g_ep_config(usb_id, MTP_BULK_EP_OUT, USB_ENDPOINT_XFER_BULK, 1, mtp_handle->ep_out_dmabuffer, maxp_size_bulkout);
    usb_g_set_intr_hander(usb_id, MTP_BULK_EP_OUT, mtp_wakeup);

    usb_enable_ep(usb_id, MTP_BULK_EP_OUT);
}
static void mtp_set_state(struct usb_device_t *usb_device, enum mtp_state state)
{
    mtp_handle->state = state;
    usb_mtp_process(usb_device);
}
static void mtp_reset(struct usb_device_t *usb_device, u32 itf_num)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    log_debug("%s", __func__);
    memset(mtp_handle, sizeof(struct usb_mtp_handle), 0);
    mtp_endpoint_init(usb_device, itf_num);
    mtp_handle->state = MTP_STATE_COMMAND;
}
u32 mtp_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num)
{
    log_debug("%s() %d", __func__, *cur_itf_num);
    //cppcheck-suppress unreadVariable
    struct usb_device_t *usb_device = usb_id2device(usb_id);
    memcpy(ptr, sMtpDescriptor, sizeof(sMtpDescriptor));
    ptr[2] = *cur_itf_num;
#if defined(FUSB_MODE) && FUSB_MODE == 0
    if (usb_device->bSpeed == USB_SPEED_FULL) {
        ptr[9 + 4] = LOBYTE(MAXP_SIZE_BULKIN_FS);
        ptr[9 + 5] = HIBYTE(MAXP_SIZE_BULKIN_FS);
        ptr[9 + 7 + 4] = LOBYTE(MAXP_SIZE_BULKOUT_FS);
        ptr[9 + 7 + 5] = HIBYTE(MAXP_SIZE_BULKOUT_FS);
    }
#endif
    if (usb_set_interface_hander(usb_id, *cur_itf_num, mtp_itf_hander) != *cur_itf_num) {

    }
    if (usb_set_reset_hander(usb_id, *cur_itf_num, mtp_reset) != *cur_itf_num) {

    }
    *cur_itf_num = *cur_itf_num + 1;
    return sizeof(sMtpDescriptor);
}
u32 ascii_to_unicode(const char *ascii_str, u16 *unicode_str)
{
    u8 offset = 0;
    while (ascii_str[offset] != 0) {
        unicode_str[offset] = ascii_str[offset];
        offset++;
    }
    unicode_str[offset] = ascii_str[offset];
    offset++;
    return offset;
}

inline static u8 __get_max_mtp_dev()
{
    u8 cnt = 0;
#if (!TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0 && TCFG_SD0_ENABLE)\
    ||(TCFG_SD0_ENABLE && TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0 && TCFG_DM_MULTIPLEX_WITH_SD_PORT != 0)
    //没有复用时候判断 sd开关
    //复用时候判断是否参与复用
    cnt++;
#endif

#if (!TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0 && TCFG_SD1_ENABLE)\
     ||(TCFG_SD1_ENABLE && TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0 && TCFG_DM_MULTIPLEX_WITH_SD_PORT != 1)
    //没有复用时候判断 sd开关
    //复用时候判断是否参与复用
    cnt++;
#endif

    //cppcheck-suppress knownConditionTrueFalse
    if (!cnt) {
        cnt = 1;
    } else if (cnt >= MAX_MTP_DEV) {
        cnt = MAX_MTP_DEV;
    }
    return cnt;
}
static void *check_disk_status(u8 cur_lun)
{
    u32 ret;
    void *dev_fd = mtp_handle->info.dev_handle[cur_lun]  ;
    const char *dev_name = mtp_handle->info.dev_name[cur_lun];

#if TCFG_DEV_MANAGER_ENABLE
    if (dev_manager_list_check_by_logo((char *)dev_name)) {
        if (dev_fd == NULL) {
            mtp_handle->info.dev_handle[cur_lun] = dev_open(dev_name, NULL);
            if (mtp_handle->info.dev_handle[cur_lun]) {
#ifdef CONFIG_EARPHONE_CASE_ENABLE
                dev_ioctl(mtp_handle->info.dev_handle[cur_lun], IOCTL_POWER_RESUME, (u32)dev_name);
#endif
            }
        } else {
            //FIXME:need add device state check??
            u32 dev_online = 0;
            ret = dev_ioctl(dev_fd, IOCTL_GET_STATUS, (u32)&dev_online);
            if (ret || !dev_online) {
                goto __dev_offline;
            }
        }

        /* if (get_cardreader_popup(cur_lun)) { */
        /*     return NULL; */
        /* } */
    } else {
        if (dev_fd) {
            goto __dev_offline;
        }
    }
#else
    if (dev_fd == NULL) {
        mtp_handle->info.dev_handle[cur_lun] = dev_open(dev_name, NULL);
    } else {
        u32 dev_online = 0;
        ret = dev_ioctl(dev_fd, IOCTL_GET_STATUS, (u32)&dev_online);
        if (ret || !dev_online) {
            goto __dev_offline;
        }
    }
#endif

    return mtp_handle->info.dev_handle[cur_lun];

__dev_offline:
    dev_close(dev_fd);
    /* recover_set_cardreader_popup(cur_lun); */
    mtp_handle->info.dev_handle[cur_lun] = NULL;
    return NULL;
}
u32 mtp_register_disk(const char *name, void *arg)
{
    int i;
    u32 ret = -1;
    if (mtp_handle) {
        for (i = 0; i < __get_max_mtp_dev(); i++) {
            if (mtp_handle->info.dev_name[i][0] == 0) {
                ASSERT(strlen(name) <= MTP_DEV_NAME_LEN, "MTP_DEV_NAME_LEN too small");
                strcpy(mtp_handle->info.dev_name[i], name);
                mtp_handle->info.dev_handle[i] = NULL;
                break;
            }
        }
        if (i == __get_max_mtp_dev()) {
            ret = -3;
        }
    }
    return ret;
}
u32 mtp_unregister_disk(const char *name)
{
    int i;
    if (mtp_handle) {
        for (i = 0; i < __get_max_mtp_dev(); i++) {
            if (!strcmp(mtp_handle->info.dev_name[i], name)) {
                mtp_handle->info.dev_name[i][0] = 0;
                return 0;
            }
        }
    }
    return -1;
}

u32 mtp_unregister_all()
{
    int i;
    if (mtp_handle) {
        for (i = 0; i < __get_max_mtp_dev(); i++) {
            if (mtp_handle->info.dev_handle[i]) {
                dev_close(mtp_handle->info.dev_handle[i]);
                mtp_handle->info.dev_handle[i] = NULL;
                /* memset(mtp_handle->info.dev_name[i], 0, MTP_DEV_NAME_LEN); */
            }
        }
    }
    return 0;
}
u32 mtp_register(const usb_dev usb_id)
{
    if (mtp_handle == NULL) {
#if USB_MALLOC_ENABLE
        mtp_handle = (struct usb_mtp_handle *)zalloc(sizeof(struct usb_mtp_handle));
        if (mtp_handle == NULL) {
            log_error("mtp_register err");
            return -1;
        }
#if USB_MTP_BULK_DEV_USE_ASYNC
        mtp_handle->mtp_buf = (u8 *)dma_malloc(MTP_BUFFER_SIZE * 2);
#else
        mtp_handle->mtp_buf = (u8 *)malloc(MTP_BUFFER_SIZE);
#endif
        if (mtp_handle->mtp_buf == NULL) {
            log_error("mtp_register err");
            return -1;
        }
#else
        memset(&_mtp_handle, 0, sizeof(struct usb_mtp_handle));
        _mtp_handle.mtp_buf = mtp_buf;
        mtp_handle = &_mtp_handle;
#endif
        log_info("mtp_handle = %x", mtp_handle);

        mtp_handle->ep_out_dmabuffer = usb_alloc_ep_dmabuffer(usb_id, MTP_BULK_EP_OUT, MAXP_SIZE_BULKIN + MAXP_SIZE_BULKOUT);

    }
    return 0;
}
u32 mtp_release()
{
    if (mtp_handle) {
        for (int i = 0; i < __get_max_mtp_dev(); i++) {
            void *dev_fd = mtp_handle->info.dev_handle[i] ;
            if (dev_fd) {
                dev_close(dev_fd);
                mtp_handle->info.dev_handle[i] = NULL;
            }
        }
        if (mtp_handle->ep_out_dmabuffer) {
            usb_free_ep_dmabuffer(0, mtp_handle->ep_out_dmabuffer);
            mtp_handle->ep_out_dmabuffer = NULL;
        }
#if USB_MALLOC_ENABLE
        if (mtp_handle->mtp_buf) {
#if USB_MTP_BULK_DEV_USE_ASYNC
            dma_free(mtp_handle->mtp_buf);
#else
            free(mtp_handle->mtp_buf);
#endif
            mtp_handle->mtp_buf = NULL;
        }
        free(mtp_handle);
#endif
        mtp_handle = NULL;
    }
    return 0;
}
u32 mtp_mcu2usb(struct usb_device_t *usb_device, const u8 *buffer, u32 len)
{
    usb_dev usb_id = usb_device2id(usb_device);
    return usb_g_bulk_write(usb_id, MTP_BULK_EP_IN, buffer, len);
}
static u32 mtp_usb2mcu(struct usb_device_t *usb_device, u8 *buffer, u32 len)
{
    usb_dev usb_id = usb_device2id(usb_device);
    return usb_g_bulk_read(usb_id, MTP_BULK_EP_OUT, buffer, len, 0);
}
static u32 mtp_command_block_fill()
{
    return 0;
}
static u32 mtp_block_fill(struct mtp_block_header *header, u16 container_type, u16 code, u32 transaction_id, const u8 *data, u32 data_len)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 offset = 0;
    u8 *buf = mtp_handle->mtp_buf;
    header->container_length = MTP_BLOCK_HEADER_SIZE + data_len;
    header->container_type = container_type;
    header->code = code;
    header->transaction_id = transaction_id;

    memcpy(&buf[offset], (u8 *)header, sizeof(struct mtp_block_header));
    offset += sizeof(struct mtp_block_header);

    memcpy(&buf[offset], data, data_len);
    offset += data_len;

    ASSERT(header->container_length == offset, "%s()\n", __func__);
    return offset;
}

u32 mtp_operation_open_session(struct usb_device_t *usb_device, u32 transaction_id)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->response;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESPONSE_OK, transaction_id, NULL, 0);
    mtp_set_state(usb_device, MTP_STATE_RESPONSE);
    return 0;
}
u32 _mtp_operation_get_device_info(u8 *data)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 offset = 0;
    u32 *ptr;

    memcpy(&data[offset], (u8 *)&DeviceInfoStandardVersion, sizeof(DeviceInfoStandardVersion));
    offset += sizeof(DeviceInfoStandardVersion);

    memcpy(&data[offset], (u8 *)&DeviceInfoVendorExtensionID, sizeof(DeviceInfoVendorExtensionID));
    offset += sizeof(DeviceInfoVendorExtensionID);

    memcpy(&data[offset], (u8 *)&DeviceInfoVendorExtensionVersion, sizeof(DeviceInfoVendorExtensionVersion));
    offset += sizeof(DeviceInfoVendorExtensionVersion);

    data[offset] = sizeof(DeviceInfoVendorExtensionDesc) / 2;
    offset += 1;
    memcpy(&data[offset], (u8 *)DeviceInfoVendorExtensionDesc, sizeof(DeviceInfoVendorExtensionDesc));
    offset += sizeof(DeviceInfoVendorExtensionDesc);

    memcpy(&data[offset], (u8 *)&DeviceInfoFunctionalMode, sizeof(DeviceInfoFunctionalMode));
    offset += sizeof(DeviceInfoFunctionalMode);

    ptr = (u32 *)&data[offset];
    *ptr = ARRAY_SIZE(DeviceInfoOperationsSupported);
    offset += 4;
    memcpy(&data[offset], (u8 *)DeviceInfoOperationsSupported, sizeof(DeviceInfoOperationsSupported));
    offset += sizeof(DeviceInfoOperationsSupported);

    ptr = (u32 *)&data[offset];
    *ptr = ARRAY_SIZE(DeviceInfoEventsSupported);
    offset += 4;
    memcpy(&data[offset], (u8 *)DeviceInfoEventsSupported, sizeof(DeviceInfoEventsSupported));
    offset += sizeof(DeviceInfoEventsSupported);

    ptr = (u32 *)&data[offset];
    *ptr = ARRAY_SIZE(DeviceInfoDevicePropertiesSupported);
    offset += 4;
    memcpy(&data[offset], (u8 *)DeviceInfoDevicePropertiesSupported, sizeof(DeviceInfoDevicePropertiesSupported));
    offset += sizeof(DeviceInfoDevicePropertiesSupported);

    ptr = (u32 *)&data[offset];
    *ptr = ARRAY_SIZE(DeviceInfoDeviceCaptureFormats);
    offset += 4;
    memcpy(&data[offset], (u8 *)DeviceInfoDeviceCaptureFormats, sizeof(DeviceInfoDeviceCaptureFormats));
    offset += sizeof(DeviceInfoDeviceCaptureFormats);

    ptr = (u32 *)&data[offset];
    *ptr = ARRAY_SIZE(DeviceInfoDeviceImageFormats);
    offset += 4;
    memcpy(&data[offset], (u8 *)DeviceInfoDeviceImageFormats, sizeof(DeviceInfoDeviceImageFormats));
    offset += sizeof(DeviceInfoDeviceImageFormats);

    data[offset] = sizeof(DeviceInfoDeviceManufacturer) / 2;
    offset += 1;
    memcpy(&data[offset], (u8 *)DeviceInfoDeviceManufacturer, sizeof(DeviceInfoDeviceManufacturer));
    offset += sizeof(DeviceInfoDeviceManufacturer);

    data[offset] = sizeof(DeviceInfoModel) / 2;
    offset += 1;
    memcpy(&data[offset], (u8 *)DeviceInfoModel, sizeof(DeviceInfoModel));
    offset += sizeof(DeviceInfoModel);

    data[offset] = sizeof(DeviceInfoDeviceVersion) / 2;
    offset += 1;
    memcpy(&data[offset], (u8 *)DeviceInfoDeviceVersion, sizeof(DeviceInfoDeviceVersion));
    offset += sizeof(DeviceInfoDeviceVersion);

    data[offset] = sizeof(DeviceInfoDeviceSerialNumber) / 2;
    offset += 1;
    memcpy(&data[offset], (u8 *)DeviceInfoDeviceSerialNumber, sizeof(DeviceInfoDeviceSerialNumber));
    offset += sizeof(DeviceInfoDeviceSerialNumber);

    put_buf(data, offset);
    return offset;
}
u32 mtp_operation_get_device_info(struct usb_device_t *usb_device, u32 transaction_id)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 data_buf[512] = {0};
    u32 data_len = _mtp_operation_get_device_info(data_buf);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_DATA, MTP_OPERATION_GET_DEVICE_INFO, transaction_id, data_buf, data_len);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
struct DevicePropDesc {
    u16 PropertyCode;
    u16 Datatype;
    u8 GetSet;
    u8 *DefaultValue;
    u8 *CurrentValue;
    u8 FormFlag;
};
const u8 DevicePropDescDefaultValue[] = {
    0,
};
const u8 DevicePropDescCurrentValue[] = {
    17,
    'J', 0x00,
    'i', 0x00,
    'e', 0x00,
    'L', 0x00,
    'i', 0x00,
    '_', 0x00,
    'M', 0x00,
    'T', 0x00,
    'P', 0x00,
    '_', 0x00,
    'D', 0x00,
    'e', 0x00,
    'v', 0x00,
    'i', 0x00,
    'c', 0x00,
    'e', 0x00,
    0x00, 0x00,
};
u32 _mtp_operation_get_device_prop_desc(u8 *data, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 offset = 0;
    u32 *ptr;

    u16 propertycode = (u16)para[0];
    struct DevicePropDesc dev_pro_desc = {
        .PropertyCode = (u16)para[0],
        .Datatype = 0xFFFF,
        .GetSet = 0x01,
        .DefaultValue = (u8 *)DevicePropDescDefaultValue,
        .CurrentValue = (u8 *)DevicePropDescCurrentValue,
        .FormFlag = 0x00,
    };

    memcpy(&data[offset], (u8 *)&dev_pro_desc.PropertyCode, sizeof(dev_pro_desc.PropertyCode));
    offset += sizeof(dev_pro_desc.PropertyCode);

    memcpy(&data[offset], (u8 *)&dev_pro_desc.Datatype, sizeof(dev_pro_desc.Datatype));
    offset += sizeof(dev_pro_desc.Datatype);

    memcpy(&data[offset], (u8 *)&dev_pro_desc.GetSet, sizeof(dev_pro_desc.GetSet));
    offset += sizeof(dev_pro_desc.GetSet);

    u8 *string = dev_pro_desc.DefaultValue;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    string = dev_pro_desc.CurrentValue;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    memcpy(&data[offset], (u8 *)&dev_pro_desc.FormFlag, sizeof(dev_pro_desc.FormFlag));
    offset += sizeof(dev_pro_desc.FormFlag);

    put_buf(data, offset);
    return offset;
}
u32 mtp_operation_get_device_prop_desc(struct usb_device_t *usb_device, u32 transaction_id, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 data_buf[512] = {0};
    u32 data_len = _mtp_operation_get_device_prop_desc(data_buf, para);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_DATA, MTP_OPERATION_GET_DEVICE_PROP_DESC, transaction_id, data_buf, data_len);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
const static u32 StorageIDArray[] = {
    0x00010001,
};
u32 _mtp_operation_get_storage_ids(u8 *data)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 offset = 0;
    u32 *ptr;

    ptr = (u32 *)&data[offset];
    *ptr = ARRAY_SIZE(StorageIDArray);
    offset += 4;
    memcpy(&data[offset], (u8 *)StorageIDArray, sizeof(StorageIDArray));
    offset += sizeof(StorageIDArray);;

    put_buf(data, offset);
    return offset;
}
u32 mtp_operation_get_storage_ids(struct usb_device_t *usb_device, u32 transaction_id)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 data_buf[512] = {0};
    u32 data_len = _mtp_operation_get_storage_ids(data_buf);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_DATA, MTP_OPERATION_GET_STORAGE_IDS, transaction_id, data_buf, data_len);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
struct StorageInfo {
    u16 StorageType;
    u16 FilesystemType;
    u16 AccessCapability;
    u64 MaxCapacity;
    u64 FreeSpaceInBytes;
    u32 FreeSpaceInImages;
    u8 *StorageDescription;
    u8 *VolumeLabel;
};
const u8 StorageInfoStorageDescription[] = {
    5,
    0x70, 0x67, //杰
    0x06, 0x74, //理
    0xd1, 0x79, //科
    0x80, 0x62, //技
    0x00, 0x00,
};
const u8 StorageInfoVolumeLabel[] = {
    10,
    'Y', 0x00,
    'o', 0x00,
    'u', 0x00,
    'b', 0x00,
    'a', 0x00,
    'i', 0x00,
    'b', 0x00,
    'a', 0x00,
    'i', 0x00,
    0x00, 0x00,
};
u32 _mtp_operation_get_storage_info(u8 *data, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 storage_id = *para;
    u8 cur_lun = 0xff;
    for (u8 i = 0; i < ARRAY_SIZE(StorageIDArray);  i++) {
        if (storage_id == StorageIDArray[i]) {
            cur_lun = i;
        }
    }
    void *dev_fd = check_disk_status(cur_lun);
    if (!dev_fd) {
        log_error("func:%s(), line:%d\n", __func__, __LINE__);
        return 0;
    }
    u32 capacity;
    dev_ioctl(dev_fd, IOCTL_GET_CAPACITY, (u32)&capacity);

    u32 offset = 0;
    u32 *ptr;

    u64 max_capacity = capacity * 512;
    u64 free_space_in_bytes = 0xFFFFFFFF;
    u32 free_space_in_images = 0xFFFFFFFF;
    struct StorageInfo storage_info = {
        .StorageType = MTP_STORAGE_FIXED_RAM,
        .FilesystemType = MTP_STORAGE_FILESYSTEM_HIERARCHICAL,
        .AccessCapability = MTP_STORAGE_READ_WRITE,
        .MaxCapacity = max_capacity,
        .FreeSpaceInBytes = free_space_in_bytes,
        .FreeSpaceInImages = free_space_in_images,
        .StorageDescription = (u8 *)StorageInfoStorageDescription,
        .VolumeLabel = (u8 *)StorageInfoVolumeLabel,
    };

    memcpy(&data[offset], (u8 *)&storage_info.StorageType, sizeof(storage_info.StorageType));
    offset += sizeof(storage_info.StorageType);

    memcpy(&data[offset], (u8 *)&storage_info.FilesystemType, sizeof(storage_info.FilesystemType));
    offset += sizeof(storage_info.FilesystemType);

    memcpy(&data[offset], (u8 *)&storage_info.AccessCapability, sizeof(storage_info.AccessCapability));
    offset += sizeof(storage_info.AccessCapability);

    memcpy(&data[offset], (u8 *)&storage_info.MaxCapacity, sizeof(storage_info.MaxCapacity));
    offset += sizeof(storage_info.MaxCapacity);

    memcpy(&data[offset], (u8 *)&storage_info.FreeSpaceInBytes, sizeof(storage_info.FreeSpaceInBytes));
    offset += sizeof(storage_info.FreeSpaceInBytes);

    memcpy(&data[offset], (u8 *)&storage_info.FreeSpaceInImages, sizeof(storage_info.FreeSpaceInImages));
    offset += sizeof(storage_info.FreeSpaceInImages);

    u8 *string = storage_info.StorageDescription;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    string = storage_info.VolumeLabel;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    put_buf(data, offset);
    return offset;
}
u32 mtp_operation_get_storage_info(struct usb_device_t *usb_device, u32 transaction_id, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 data_buf[512] = {0};
    u32 data_len = _mtp_operation_get_storage_info(data_buf, para);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_DATA, MTP_OPERATION_GET_STORAGE_INFO, transaction_id, data_buf, data_len);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
/* const static u32 ObjectHandleArray[] = { */
/*     0x00000001, */
/* }; */
static const char scan_parm[] = "-t"
#if (TCFG_DEC_MP3_ENABLE)
                                "MP1MP2MP3"
#endif
#if 1//( TCFG_DEC_WAV_ENABLE)
                                "WAV"
#endif
                                " -sn -r"
                                ;
u32 _mtp_operation_get_object_handles(u8 *data, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u32 storage_id = *para;
    u8 cur_lun = 0xff;
    for (u8 i = 0; i < ARRAY_SIZE(StorageIDArray);  i++) {
        if (storage_id == StorageIDArray[i]) {
            cur_lun = i;
        }
    }
    void *dev_fd = check_disk_status(cur_lun);
    if (!dev_fd) {
        log_error("func:%s(), line:%d\n", __func__, __LINE__);
        return 0;
    }

    const char *dev_name = mtp_handle->info.dev_name[cur_lun];
    struct __dev *dev = dev_manager_list_check_by_logo((char *)dev_name);
    mtp_handle->info.dev = dev;
    struct vfscan *fsn = file_manager_scan_disk(dev, NULL, scan_parm, FCYCLE_ALL, NULL);
    mtp_handle->info.fsn = fsn;
    log_info("file_number %d\n", fsn->file_number);



    u32 offset = 0;
    u32 *ptr;

    /* ptr = (u32 *)&data[offset]; */
    /* *ptr = ARRAY_SIZE(ObjectHandleArray); */
    /* offset += 4; */
    /* memcpy(&data[offset], (u8 *)ObjectHandleArray, sizeof(ObjectHandleArray)); */
    /* offset += sizeof(ObjectHandleArray); */

    ptr = (u32 *)&data[offset];
    *ptr = fsn->file_number;
    offset += 4;
    for (u32 i = 1; i <= fsn->file_number; i++) {
        memcpy(&data[offset], (u8 *)&i, 4);
        offset += 4;
    }

    put_buf(data, offset);
    return offset;
}
u32 mtp_operation_get_object_handles(struct usb_device_t *usb_device, u32 transaction_id, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 data_buf[512] = {0};
    u32 data_len = _mtp_operation_get_object_handles(data_buf, para);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_DATA, MTP_OPERATION_GET_OBJECT_HANDLES, transaction_id, data_buf, data_len);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
struct ObjectInfo {
    u32 StorageID;
    u16 ObjectFormat;
    u16 ProtectionStatus;
    u32 ObjectCompressedSize;
    u16 ThumbFormat;
    u32 ThumbCompressedSize;
    u32 ThumbPixWidth;
    u32 ThumbPixHeight;
    u32 ImagePixWidth;
    u32 ImagePixHeight;
    u32 ImageBitDepth;
    u32 ParentObject;
    u16 AssociationType;
    u32 AssociationDescription;
    u32 SequenceNumber;
    u8 *Filename;
    u8 *DateCreated; //CaptureDate
    u8 *DateModified; //ModificationDate
    u8 *Keywords;
};
const static u8 ObjectInfoFilename[] = {
    4,
    'Y', 0x00,
    'b', 0x00,
    'b', 0x00,
    0x00, 0x00,
};
const static u32 ObjectInfoDateCreated[] = {
};
const static u32 ObjectInfoDateModified[] = {
};
const static u32 ObjectInfoKeywords[] = {
};
u32 _mtp_operation_get_object_info(u8 *data, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    struct __dev *dev = mtp_handle->info.dev;
    struct vfscan *fsn = mtp_handle->info.fsn;

    FILE *file = fselect(fsn, FSEL_BY_NUMBER, (int) * para); //选择指定文件
    if (file == NULL) {
        log_error("error i = %d\n", *para);
        return 0;
    }
    u8 file_name[64] = {0};
    u32 strings_num = fget_name(file, file_name, sizeof(file_name));
    /* printf("fget_name:\n"); */
    /* put_buf((u8 *)file_name, strings_num); */
    u16 Filename[32] = {0};
    if (utf8_check((char *)file_name, strings_num)) {
        strings_num = UTF82Unicode((char *)file_name, Filename, strings_num);
        strings_num  += 1;
    } else if ((file_name[0] == 0x5c) && (file_name[1] == 0x55)) {
        strings_num -= 2;
        memcpy(Filename, &file_name[2], strings_num);
        strings_num  = strings_num / 2 + 1;
    } else {
        memcpy(Filename, file_name, strings_num);
        strings_num  = strings_num / 2 + 1;
    }
    log_info("file_name buf: %d\n", *para);
    put_buf((u8 *)Filename, strings_num * 2);

    u32 FileSize = flen(file);
    log_info("FileSize:%dByte\n", FileSize);
    fclose(file);


    u32 offset = 0;
    u32 *ptr;

    struct ObjectInfo obj_info = {
        .StorageID = 0x1,
        .ObjectFormat = 0x3000,
        .ProtectionStatus = 0,
        .ObjectCompressedSize = FileSize,//10 * 1024,
        .ThumbFormat = 0x3000,
        .ThumbCompressedSize = 00,
        .ThumbPixWidth = 0,
        .ThumbPixHeight = 0,
        .ImagePixWidth = 0,
        .ImagePixHeight = 0,
        .ImageBitDepth = 0,
        .ParentObject = 0,
        .AssociationType = 0x0001,
        .AssociationDescription = 0,
        .SequenceNumber = 0,
        .Filename = (u8 *)Filename,//(u8 *)ObjectInfoFilename,
        .DateCreated = (u8 *)ObjectInfoDateCreated,
        .DateModified = (u8 *)ObjectInfoDateModified,
        .Keywords = (u8 *)ObjectInfoKeywords,
    };

    memcpy(&data[offset], (u8 *)&obj_info.StorageID, sizeof(obj_info.StorageID));
    offset += sizeof(obj_info.StorageID);

    memcpy(&data[offset], (u8 *)&obj_info.ObjectFormat, sizeof(obj_info.ObjectFormat));
    offset += sizeof(obj_info.ObjectFormat);

    memcpy(&data[offset], (u8 *)&obj_info.ProtectionStatus, sizeof(obj_info.ProtectionStatus));
    offset += sizeof(obj_info.ProtectionStatus);

    memcpy(&data[offset], (u8 *)&obj_info.ObjectCompressedSize, sizeof(obj_info.ObjectCompressedSize));
    offset += sizeof(obj_info.ObjectCompressedSize);

    memcpy(&data[offset], (u8 *)&obj_info.ThumbFormat, sizeof(obj_info.ThumbFormat));
    offset += sizeof(obj_info.ThumbFormat);

    memcpy(&data[offset], (u8 *)&obj_info.ThumbCompressedSize, sizeof(obj_info.ThumbCompressedSize));
    offset += sizeof(obj_info.ThumbCompressedSize);

    memcpy(&data[offset], (u8 *)&obj_info.ThumbPixWidth, sizeof(obj_info.ThumbPixWidth));
    offset += sizeof(obj_info.ThumbPixWidth);

    memcpy(&data[offset], (u8 *)&obj_info.ThumbPixHeight, sizeof(obj_info.ThumbPixHeight));
    offset += sizeof(obj_info.ThumbPixHeight);

    memcpy(&data[offset], (u8 *)&obj_info.ImagePixWidth, sizeof(obj_info.ImagePixWidth));
    offset += sizeof(obj_info.ImagePixWidth);

    memcpy(&data[offset], (u8 *)&obj_info.ImagePixHeight, sizeof(obj_info.ImagePixHeight));
    offset += sizeof(obj_info.ImagePixHeight);

    memcpy(&data[offset], (u8 *)&obj_info.ImageBitDepth, sizeof(obj_info.ImageBitDepth));
    offset += sizeof(obj_info.ImageBitDepth);

    memcpy(&data[offset], (u8 *)&obj_info.ParentObject, sizeof(obj_info.ParentObject));
    offset += sizeof(obj_info.ParentObject);

    memcpy(&data[offset], (u8 *)&obj_info.AssociationType, sizeof(obj_info.AssociationType));
    offset += sizeof(obj_info.AssociationType);

    memcpy(&data[offset], (u8 *)&obj_info.AssociationDescription, sizeof(obj_info.AssociationDescription));
    offset += sizeof(obj_info.AssociationDescription);

    memcpy(&data[offset], (u8 *)&obj_info.SequenceNumber, sizeof(obj_info.SequenceNumber));
    offset += sizeof(obj_info.SequenceNumber);

    u8 *string = obj_info.Filename;
    data[offset] = (u8)strings_num;//string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[0], strings_num * 2);
    offset += strings_num * 2;

    string = obj_info.DateCreated;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    string = obj_info.DateModified;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    string = obj_info.Keywords;
    data[offset] = string[0];
    offset += 1;
    memcpy(&data[offset], (u8 *)&string[1], string[0] * 2);
    offset += string[0] * 2;

    put_buf(data, offset);
    return offset;
}
u32 mtp_operation_get_object_info(struct usb_device_t *usb_device, u32 transaction_id, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 data_buf[512] = {0};
    u32 data_len = _mtp_operation_get_object_info(data_buf, para);
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    mtp_block_fill(header, MTP_CONTAINER_TYPE_DATA, MTP_OPERATION_GET_OBJECT_INFO, transaction_id, data_buf, data_len);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
u32 object_data_offset;
u32 _mtp_operation_get_object(u8 *data, u32 *para, u32 len)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);

    struct vfscan *fsn = mtp_handle->info.fsn;
    FILE *file = fselect(fsn, FSEL_BY_NUMBER, (int) * para); //选择指定文件
    if (file == NULL) {
        log_error("error i = %d\n", *para);
        return 0;
    }
#if 1
    fseek(file, object_data_offset, SEEK_SET);
    u32 rlen  = fread(data, len, 1, file);
    object_data_offset += rlen;
    fclose(file);
#else //test
    u8 buf[MTP_BUFFER_SIZE];
    u32 offset = 0;
    u32 rlen;
    for (u32 i = 0; i < 1000; i++) {
        file = fselect(fsn, FSEL_BY_NUMBER, (int) * para); //选择指定文件
        fseek(file, offset, SEEK_SET);
        rlen  = fread(buf, MTP_BUFFER_SIZE, 1, file);
        offset += MTP_BUFFER_SIZE;
        fclose(file);
        wdt_clear();
    }
    while (1);
#endif

    return rlen;
}
/* static void mtp_data_tx_cb(struct usb_device_t *usb_device, u32 ep) */
/* { */
/*     putchar('R'); */
/*     #<{(| printf("mtp_data_tx_cb\n"); |)}># */
/*     mtp_wakeup(usb_device, MTP_BULK_EP_IN); */
/* } */
u32 mtp_operation_get_object(struct usb_device_t *usb_device, u32 transaction_id, u32 *para)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    struct vfscan *fsn = mtp_handle->info.fsn;
    FILE *file = fselect(fsn, FSEL_BY_NUMBER, (int) * para); //选择指定文件
    object_data_offset = 0;
    struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->data;
    header->container_length = flen(file) + MTP_BLOCK_HEADER_SIZE;
    header->container_type = MTP_CONTAINER_TYPE_DATA;
    header->code = MTP_OPERATION_GET_OBJECT;
    header->transaction_id = transaction_id;
    fclose(file);

    u8 *buf = mtp_handle->mtp_buf;
    memcpy(&buf[0], (u8 *)header, sizeof(struct mtp_block_header));
    u32 data_len = (header->container_length < MTP_BUFFER_SIZE ? header->container_length : MTP_BUFFER_SIZE);
    u32 rlen = _mtp_operation_get_object(&buf[MTP_BLOCK_HEADER_SIZE], para, data_len - MTP_BLOCK_HEADER_SIZE);
    const usb_dev usb_id = usb_device2id(usb_device);
    /* usb_g_set_intr_hander(usb_id, USB_DIR_IN|MTP_BULK_EP_IN, mtp_data_tx_cb); */
    /* usb_set_intr_txe(usb_id, MTP_BULK_EP_IN); */
    mtp_mcu2usb(usb_device, buf, rlen + MTP_BLOCK_HEADER_SIZE);
    mtp_set_state(usb_device, MTP_STATE_DATA_TX);
    return 0;
}
static u32 mtp_state_command(struct usb_device_t *usb_device)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    u8 packet[64];
    u32 rx_len = mtp_usb2mcu(usb_device, packet, sizeof(packet));
    u16 ContainerType = ((packet[MTP_CONTAINER_TYPE_OFFSET + 1] << 8) | packet[MTP_CONTAINER_TYPE_OFFSET]);
    if (ContainerType != MTP_CONTAINER_TYPE_COMMAND) {
        log_error("func:%s(), line:%d\n", __func__, __LINE__);
        return -1;
    }
    struct mtp_command_block *cmd_block = &mtp_handle->command;
    memset(cmd_block, 0, sizeof(struct mtp_command_block));
    memcpy((u8 *)cmd_block, packet, rx_len);
    mtp_command_block_dump();

    switch (cmd_block->code) {
    case MTP_OPERATION_OPEN_SESSION:
        mtp_operation_open_session(usb_device, cmd_block->transaction_id);
        break;
    case MTP_OPERATION_GET_DEVICE_INFO:
        mtp_operation_get_device_info(usb_device, cmd_block->transaction_id);
        break;
    case MTP_OPERATION_GET_DEVICE_PROP_DESC:
        mtp_operation_get_device_prop_desc(usb_device, cmd_block->transaction_id, &cmd_block->parameter1);
        break;
    case MTP_OPERATION_GET_STORAGE_IDS:
        mtp_operation_get_storage_ids(usb_device, cmd_block->transaction_id);
        break;
    case MTP_OPERATION_GET_STORAGE_INFO:
        mtp_operation_get_storage_info(usb_device, cmd_block->transaction_id, &cmd_block->parameter1);
        break;
    case MTP_OPERATION_GET_OBJECT_HANDLES:
        mtp_operation_get_object_handles(usb_device, cmd_block->transaction_id, &cmd_block->parameter1);
        break;
    case MTP_OPERATION_GET_OBJECT_INFO:
        mtp_operation_get_object_info(usb_device, cmd_block->transaction_id, &cmd_block->parameter1);
        break;
    case MTP_OPERATION_GET_OBJECT:
        mtp_operation_get_object(usb_device, cmd_block->transaction_id, &cmd_block->parameter1);
        break;
    default:
        log_error("func:%s(), line:%d\n", __func__, __LINE__);
        break;
    }
    return 0;
}
static u32 mtp_state_data_tx(struct usb_device_t *usb_device)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    switch (mtp_handle->command.code) {
    case MTP_OPERATION_GET_DEVICE_INFO:
    case MTP_OPERATION_GET_DEVICE_PROP_DESC:
    case MTP_OPERATION_GET_STORAGE_IDS:
    case MTP_OPERATION_GET_STORAGE_INFO:
    case MTP_OPERATION_GET_OBJECT_HANDLES:
    case MTP_OPERATION_GET_OBJECT_INFO:
        mtp_mcu2usb(usb_device, mtp_handle->mtp_buf, mtp_handle->data.container_length);
        struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->response;
        mtp_block_fill(header, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESPONSE_OK, mtp_handle->data.transaction_id, NULL, 0);
        mtp_set_state(usb_device, MTP_STATE_RESPONSE);
        break;
    case MTP_OPERATION_GET_OBJECT:
        if (object_data_offset == mtp_handle->data.container_length - MTP_BLOCK_HEADER_SIZE) {
            const usb_dev usb_id = usb_device2id(usb_device);
            /* usb_clr_intr_txe(usb_id, MTP_BULK_EP_IN); */
            struct mtp_block_header *header = (struct mtp_block_header *)&mtp_handle->response;
            mtp_block_fill(header, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESPONSE_OK, mtp_handle->data.transaction_id, NULL, 0);
            mtp_set_state(usb_device, MTP_STATE_RESPONSE);
            break;
        }
        u32 rlen = _mtp_operation_get_object(mtp_handle->mtp_buf, &(mtp_handle->command.parameter1), MTP_BUFFER_SIZE);
        mtp_mcu2usb(usb_device, mtp_handle->mtp_buf, rlen);
        mtp_wakeup(usb_device, MTP_BULK_EP_IN);
        break;
    default:
        log_error("func:%s(), line:%d\n", __func__, __LINE__);
        break;
    }
    return 0;
}
static u32 mtp_state_data_rx(struct usb_device_t *usb_device)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    switch (mtp_handle->command.code) {
    default:
        log_error("func:%s(), line:%d\n", __func__, __LINE__);
        break;
    }
    return 0;
}
static u32 mtp_state_response(struct usb_device_t *usb_device)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    mtp_mcu2usb(usb_device, mtp_handle->mtp_buf, mtp_handle->response.container_length);
    mtp_handle->state = MTP_STATE_COMMAND;
    return 0;
}
void usb_mtp_process(struct usb_device_t *usb_device)
{
    log_debug("func:%s(), line:%d\n", __func__, __LINE__);
    switch (mtp_handle->state) {
    case MTP_STATE_IDLE:
        log_error("MTP not init!!!\n");
        break;
    case MTP_STATE_COMMAND:
        mtp_state_command(usb_device);
        break;
    case MTP_STATE_DATA_TX:
        mtp_state_data_tx(usb_device);
        break;
    case MTP_STATE_DATA_RX:
        mtp_state_data_rx(usb_device);
        break;
    case MTP_STATE_RESPONSE:
        mtp_state_response(usb_device);
        break;
    default:
        break;
    }
}
#endif

