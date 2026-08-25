#ifndef RDX_KEY_ADAPTER_BR56_H
#define RDX_KEY_ADAPTER_BR56_H

/*
 * Handle an encoded MSG_FROM_KEY payload.
 *
 * Return 1 when the key is owned and consumed by RDX, or 0 when the key must
 * continue through the native application-mode key table.
 */
int rdx_key_adapter_handle(int *msg);

#endif
