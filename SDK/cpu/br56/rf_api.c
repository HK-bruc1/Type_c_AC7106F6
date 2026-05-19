#include "gpio.h"

void rf_pa_io_set(u8 tx_io, u8 rx_io, u8 en)
{
    //MCLR_EN(0);
    if (en) {
        gpio_och_sel_output_signal(tx_io, OCH_WL_AMPE);
        gpio_och_sel_output_signal(rx_io, OCH_WL_LNAE);
    } else {
        gpio_och_disable_output_signal(tx_io, OCH_WL_AMPE);
        gpio_och_disable_output_signal(rx_io, OCH_WL_LNAE);
    }
    gpio_hw_set_direction(IO_PORT_SPILT(tx_io), 0);
    gpio_hw_write(tx_io, 0);
    gpio_hw_set_direction(IO_PORT_SPILT(rx_io), 0);
    gpio_hw_write(rx_io, 0);

}


