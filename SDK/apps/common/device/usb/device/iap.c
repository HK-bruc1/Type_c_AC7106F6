#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".usb.data.bss")
#pragma data_seg(".usb.data")
#pragma code_seg(".usb.text")
#pragma const_seg(".usb.text.const")
#pragma str_literal_override(".usb.text.const")
#endif

#include "usb/device/usb_stack.h"
#include "usb/usb_config.h"
#include "usb/device/iap.h"
#include "app_config.h"
#include "os/os_api.h"

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[IAP_USB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_USB_APPLE_DOCK_EN

static u8 *iap_ep_in_dma;
static u8 *iap_ep_out_dma;
static void (*iap_wakeup)(struct usb_device_t *usb_device);

static const u8 iAPDescriptor[] = {  //<Interface & Endpoint Descriptor
///Interface
    USB_DT_INTERFACE_SIZE,   //Length
    USB_DT_INTERFACE,   //DescriptorType:Inerface
    0x00,   //InterfaceNum:0
    0x00,   //AlternateSetting:0
    0x02,   //NumEndpoint:0
    0xff,   //InterfaceClass:MSD
    0xf0,   //InterfaceSubClass SCSI transparent command set Allocated by USB-IF for SCSI. SCSI standards are defined outside of USB.
    0x00,   //InterfaceProtocol BBB USB Mass Storage Class Bulk-Only (BBB) Transport
    IAP_STR_INDEX,   //Interface String
///Endpoint IN
    USB_DT_ENDPOINT_SIZE,
    USB_DT_ENDPOINT,
    USB_DIR_IN | MSD_BULK_EP_IN,
    USB_ENDPOINT_XFER_BULK,
    LOBYTE(MAXP_SIZE_BULKIN), HIBYTE(MAXP_SIZE_BULKIN),
    0x01,
///Endpoint OUT
    USB_DT_ENDPOINT_SIZE,
    USB_DT_ENDPOINT,
    MSD_BULK_EP_OUT,
    USB_ENDPOINT_XFER_BULK,
    LOBYTE(MAXP_SIZE_BULKOUT), HIBYTE(MAXP_SIZE_BULKOUT),
    0x01,
};

void iap_set_wakeup_handler(void (*handle)(struct usb_device_t *usb_device))
{
    iap_wakeup = handle;
}

static void iap_wakeup_handler(struct usb_device_t *usb_device, u32 ep)
{
    if (iap_wakeup) {
        iap_wakeup(usb_device);
    }
}

static void iap_endpoint_init(struct usb_device_t *usb_device, u32 itf)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    usb_g_ep_config(usb_id, MSD_BULK_EP_IN | USB_DIR_IN, USB_ENDPOINT_XFER_BULK, 0, iap_ep_in_dma, MAXP_SIZE_BULKIN);
    usb_g_ep_config(usb_id, MSD_BULK_EP_OUT, USB_ENDPOINT_XFER_BULK, 0, iap_ep_out_dma, MAXP_SIZE_BULKOUT);
    usb_g_set_intr_hander(usb_id, MSD_BULK_EP_OUT, iap_wakeup_handler);
    usb_enable_ep(usb_id, MSD_BULK_EP_OUT);
}

static u32 iap_itf_hander(struct usb_device_t *usb_device, struct usb_ctrlrequest *setup)
{
    return 0;
}

static void iap_reset_wakeup(struct usb_device_t *usb_device, u32 itf_num)
{
    iap_endpoint_init(usb_device, itf_num);
}

void iap_register(const usb_dev usb_id)
{
    if (iap_ep_in_dma == NULL) {
        iap_ep_in_dma = usb_alloc_ep_dmabuffer(usb_id, MSD_BULK_EP_IN | USB_DIR_IN, MAXP_SIZE_BULKIN);
        ASSERT(iap_ep_in_dma, "usb alloc iap ep buffer failed");
    }
    if (iap_ep_out_dma == NULL) {
        iap_ep_out_dma = usb_alloc_ep_dmabuffer(usb_id, MSD_BULK_EP_OUT, MAXP_SIZE_BULKOUT);
        ASSERT(iap_ep_out_dma, "usb alloc iap ep buffer failed");
    }
}

void iap_release(const usb_dev usb_id)
{
    if (iap_ep_in_dma) {
        usb_free_ep_dmabuffer(usb_id, iap_ep_in_dma);
        iap_ep_in_dma = NULL;
    }
    if (iap_ep_out_dma) {
        usb_free_ep_dmabuffer(usb_id, iap_ep_out_dma);
        iap_ep_out_dma = NULL;
    }
}

u32 iap_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num)
{
    log_info("%s() %d", __func__, *cur_itf_num);
    memcpy(ptr, iAPDescriptor, sizeof(iAPDescriptor));
    ptr[2] = *cur_itf_num;
    if (usb_set_interface_hander(usb_id, *cur_itf_num, iap_itf_hander) != *cur_itf_num) {

    }
    if (usb_set_reset_hander(usb_id, *cur_itf_num, iap_reset_wakeup) != *cur_itf_num) {

    }
    *cur_itf_num = *cur_itf_num + 1;
    return sizeof(iAPDescriptor);
}

void apple_usb_iap_wakeup(struct usb_device_t *usb_device)
{
    iap_wakeup_handler(usb_device, MSD_BULK_EP_IN);
}

u8 MCU_SRAMToUSB_app(void *hdl, u8 *pBuf, u16 uCount)
{
    usb_dev usb_id = usb_device2id(hdl);
    return usb_g_bulk_write(usb_id, MSD_BULK_EP_IN, pBuf, uCount);
}

u8 MCU_USBToSRAM_app(void *hdl, u8 *pBuf, u16 uCount)
{
    usb_dev usb_id = usb_device2id(hdl);
    int ret;
    int cnt = 10000;
    while (cnt--) {
        ret = usb_g_bulk_read(usb_id, MSD_BULK_EP_OUT, pBuf, uCount, 0);
        if (ret) {
            break;
        }
    }
    return ret;
}

#endif


