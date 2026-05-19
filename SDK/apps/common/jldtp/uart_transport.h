#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H


#include "jldtp/jldtp.h"


typedef struct {
    u32 tx_pin;
    u32 rx_pin;
    u32 baud_rate;
    u16 mtu_size;
} jldtp_uart_config_t;

extern const jldtp_transport_t jldtp_uart_transport;







#endif

