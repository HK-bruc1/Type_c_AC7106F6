#include "system/init.h"
#include "generic/printf.h"

extern char __VERSION_BEGIN[];
extern char __VERSION_END[];
extern const char *get_lib_aec_version();
extern const char *get_lib_media_version();
extern const char *get_lib_system_version();
extern const char *get_lib_update_version();
extern const char *get_lib_driver_version();
extern const char *get_lib_utils_version();
extern const char *get_lib_btctrler_version();
extern const char *get_lib_btstack_version();

__INITCALL_BANK_CODE
static int sdk_version_check()
{
    puts("=================Version===============\n");
    for (char *version = __VERSION_BEGIN; version < __VERSION_END;) {
        version += 4;
        printf("%s\n", version);
        version += strlen(version) + 1;
    }
#ifndef __SHELL__
    /* printf("%s\n", get_lib_aec_version() + 4); */
    /* printf("%s\n", get_lib_media_version() + 4); */
    /* printf("%s\n", get_lib_system_version() + 4); */
    /* printf("%s\n", get_lib_update_version() + 4); */
    /* printf("%s\n", get_lib_driver_version() + 4); */
    /* printf("%s\n", get_lib_utils_version() + 4); */
    /* printf("%s\n", get_lib_btctrler_version() + 4); */
    /* printf("%s\n", get_lib_btstack_version() + 4); */
#endif
    puts("=======================================\n");

    return 0;
}
early_initcall(sdk_version_check);
