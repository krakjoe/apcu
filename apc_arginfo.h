#if PHP_VERSION_ID < 80000
# include "php_apc_legacy_arginfo.h"
#else
# error Not supported on PHP >= 8.0
#endif
