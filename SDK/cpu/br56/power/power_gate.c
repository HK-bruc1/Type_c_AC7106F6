#include "system/includes.h"
#include "cpu/includes.h"
#include "app_config.h"



void sdpg_config(int enable)
{
    if (enable == 0) {                  //断电，IO设高阻
        JL_IOMC->STPG_CON |=  BIT(0);
        gpio_set_drive_strength(IO_PORT_SPILT(IO_PORTP_02), PORT_DRIVE_STRENGT_64p0mA);
        udelay(1);
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_02), PORT_OUTPUT_LOW);
        mdelay(2);
        JL_IOMC->STPG_CON &= ~BIT(0);
        gpio_set_drive_strength(IO_PORT_SPILT(IO_PORTP_02), PORT_DRIVE_STRENGT_2p4mA);
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_02), PORT_HIGHZ);

    } else if (enable <= 4) {           //供电，IO_HD常开
        if (gpio_get_drive_strength(IO_PORT_SPILT(IO_PORTP_02))) {
            return;
        }
        JL_IOMC->STPG_CON |=  BIT(0);
        gpio_set_drive_strength(IO_PORT_SPILT(IO_PORTP_02), PORT_DRIVE_STRENGT_64p0mA);
        udelay(1);
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_02), PORT_OUTPUT_HIGH);
        mdelay(4);
        JL_IOMC->STPG_CON &= ~BIT(0);

    } else {                            //断电，IO输出0
        JL_IOMC->STPG_CON |=  BIT(0);
        gpio_set_drive_strength(IO_PORT_SPILT(IO_PORTP_02), PORT_DRIVE_STRENGT_64p0mA);
        udelay(1);
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_02), PORT_OUTPUT_LOW);
        mdelay(2);
        JL_IOMC->STPG_CON &= ~BIT(0);
        gpio_set_drive_strength(IO_PORT_SPILT(IO_PORTP_02), PORT_DRIVE_STRENGT_2p4mA);
    }
}
