#ifndef RDX_PRODUCT_KEY_ROUTING_H
#define RDX_PRODUCT_KEY_ROUTING_H

/*
 * Handle an encoded MSG_FROM_KEY payload.
 *
 * Return 1 when the key is owned and consumed by RDX, or 0 when the key must
 * continue through the native application-mode key table.
 */
int rdx_product_key_routing_handle(int *msg);

#endif
