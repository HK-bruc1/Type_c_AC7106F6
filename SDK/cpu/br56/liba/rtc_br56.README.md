# BR56 RTC library provenance

`rtc_br56.a` contains only the AC710N/BR56 RTC implementation needed by this
project. It was extracted from `AC710N_TWS` tag `3.12.6` (`deb68ce`) instead of
replacing this project's complete `cpu.a`.

Included objects:

- `osc_32k.c.o`
- `rtc_lptmr_hw_v4.c.o`
- `datetime_utils.c.o`
- `rtc_dev.c.o`
- `rtc_lptmr.c.o`

Archive SHA256:

`5F0E6ED5EA0570D49479B6A0BBEF42C5D90D7F524736CAC64D56F69E4AD70497`

The archive must remain before `cpu/br56/liba/cpu.a` in the link group so the
new RTC device implementation is selected while all unrelated BR56 drivers
continue to come from the original project library.
