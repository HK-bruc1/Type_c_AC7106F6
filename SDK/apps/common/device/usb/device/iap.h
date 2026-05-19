#ifndef __IAP_USB_H__
#define __IAP_USB_H__

#include "usb/device/usb_stack.h"

#ifdef __cplusplus
extern "C" {
#endif

u32 iap_desc_config(const usb_dev usb_id, u8 *ptr, u32 *itf);
void iap_set_wakeup_handler(void (*handle)(struct usb_device_t *usb_device));
void apple_usb_iap_wakeup(struct usb_device_t *usb_device);
void iap_register(const usb_dev usb_id);
void iap_release(const usb_dev usb_id);

#ifdef __cplusplus
}
#endif

#endif
