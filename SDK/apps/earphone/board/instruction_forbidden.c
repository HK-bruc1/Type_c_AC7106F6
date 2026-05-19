#include "typedef.h"

#define df_dummy_info "native function is not directly supported by hardware"

#define UNSUPPORTED_DF_FUNC(func_name)  \
    void func_name(void) {              \
        ASSERT(0, df_dummy_info);       \
    }

#if 0
// Now define all the unsupported double-float functions:
UNSUPPORTED_DF_FUNC(__floatsidf)
UNSUPPORTED_DF_FUNC(__fixdfsi)
UNSUPPORTED_DF_FUNC(__udivdi3)
UNSUPPORTED_DF_FUNC(__divdf3)
UNSUPPORTED_DF_FUNC(__adddf3)
UNSUPPORTED_DF_FUNC(__muldf3)
UNSUPPORTED_DF_FUNC(__subdf3)
UNSUPPORTED_DF_FUNC(__truncdfsf2)
UNSUPPORTED_DF_FUNC(__extendsfdf2)
UNSUPPORTED_DF_FUNC(__ledf2)
UNSUPPORTED_DF_FUNC(__cmpdf2)
UNSUPPORTED_DF_FUNC(__gedf2)
UNSUPPORTED_DF_FUNC(__unorddf2)
UNSUPPORTED_DF_FUNC(__eqdf2)
UNSUPPORTED_DF_FUNC(__ltdf2)
UNSUPPORTED_DF_FUNC(__nedf2)
UNSUPPORTED_DF_FUNC(__gtdf2)
#endif

