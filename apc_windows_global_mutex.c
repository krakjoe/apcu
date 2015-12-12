/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Pierre Joye <pierre@php.net>                                |
  +----------------------------------------------------------------------+
 */

#include <php.h>
#include <zend.h>
#include "ext/standard/md5.h"

#ifdef APC_WINDOWS_GLOBAL_MUTEX
#include "php_apc.h"
#include "apc_windows_global_mutex.h"

#define APCu_MUTEX_NAME "APCuMutex"

ZEND_TLS uint32_t mutex_counter = 0;

/* stollen the system_id function from 7.0 Opcache */
#define APCU_BUILD_ID PHP_APCU_VERSION ZEND_TOSTR(ZEND_EXTENSION_API_NO) ZEND_BUILD_TS ZEND_BUILD_DEBUG ZEND_BUILD_SYSTEM ZEND_BUILD_EXTRA 
#define APCU_BIN_ID "BIN_" ZEND_TOSTR(SIZEOF_CHAR) ZEND_TOSTR(SIZEOF_INT) ZEND_TOSTR(SIZEOF_LONG) ZEND_TOSTR(SIZEOF_SIZE_T) ZEND_TOSTR(SIZEOF_ZEND_LONG) ZEND_TOSTR(ZEND_MM_ALIGNMENT)
static char *apc_gen_system_id(void)
{
        PHP_MD5_CTX context;
        unsigned char digest[16], c;
        int i;
        static char md5str[33];
        static zend_bool done = 0;

        if (done) {
                return md5str;
        }

        PHP_MD5Init(&context);
        PHP_MD5Update(&context, PHP_VERSION, sizeof(PHP_VERSION)-1);
        PHP_MD5Update(&context, APCU_BUILD_ID, sizeof(APCU_BUILD_ID)-1);
        PHP_MD5Update(&context, APCU_BIN_ID, sizeof(APCU_BIN_ID)-1);
        if (strstr(PHP_VERSION, "-dev") != 0) {
                /* Development versions may be changed from build to build */
                PHP_MD5Update(&context, __DATE__, sizeof(__DATE__)-1);
                PHP_MD5Update(&context, __TIME__, sizeof(__TIME__)-1);
        }
        PHP_MD5Final(digest, &context);
        for (i = 0; i < 16; i++) {
                c = digest[i] >> 4;
                c = (c <= 9) ? c + '0' : c - 10 + 'a';
                md5str[i * 2] = c;
                c = digest[i] &  0x0f;
                c = (c <= 9) ? c + '0' : c - 10 + 'a';
                md5str[(i * 2) + 1] = c;
        }

        md5str[32] = '\0';
        done = 1;

        return md5str;
}


static char *apc_windows_create_identifyer(char *base, size_t base_len, char *out, size_t out_len)
{
    snprintf(out, out_len, "%s.%u.%s", base, mutex_counter, apc_gen_system_id());

    mutex_counter++;

    return out;
}

apc_windows_global_mutex_t *apc_windows_global_mutex_create(apc_windows_global_mutex_t *obj)
{

    apc_windows_create_identifyer(APCu_MUTEX_NAME, sizeof(APCu_MUTEX_NAME), obj->mutex_name, sizeof(obj->mutex_name));
    obj->mutex = CreateMutex(NULL, FALSE, obj->mutex_name);

    if (!obj->mutex) {
        return NULL;
    }

    /*if (GetLastError() == ERROR_ALREADY_EXISTS) {
        DWORD res;
        res = WaitForSingleObject(obj->mutex, INFINITE);

        if (WAIT_FAILED == res) {
            CloseHandle(obj->mutex);
            return NULL;
        }
    }*/

    ReleaseMutex(obj->mutex);

    return obj;
}

zend_bool apc_windows_global_mutex_lock(apc_windows_global_mutex_t *obj)
{
    DWORD res;

    res = WaitForSingleObject(obj->mutex, INFINITE);

    if (WAIT_FAILED == res) {
        return 0;
    }

    return 1;
}

zend_bool apc_windows_global_mutex_unlock(apc_windows_global_mutex_t *obj)
{
    return ReleaseMutex(obj->mutex);
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
