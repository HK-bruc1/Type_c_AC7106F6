#include "asm/power_interface.h"
#include "iokey.h"
#include "irkey.h"
#include "adkey.h"
#include "app_config.h"

void gpio_config_soft_poweroff(void)
{
    PORT_TABLE(g);

#if TCFG_IOKEY_ENABLE
    PORT_PROTECT(get_iokey_power_io());
#endif

#if TCFG_ADKEY_ENABLE
    PORT_PROTECT(get_adkey_io());
#endif

    __port_init((u32)gpio_config);
}
