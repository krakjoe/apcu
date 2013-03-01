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
  | Authors: Dmitry Stogov <dmitry@zend.com>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_string.c 328965 2013-01-03 12:46:05Z remi $ */

#include "apc.h"
#include "apc_globals.h"
#include "apc_php.h"
#include "apc_lock.h"

#ifdef ZEND_ENGINE_2_4

#ifndef ZTS
typedef struct _apc_interned_strings_data_t {
    char *interned_strings_start;
    char *interned_strings_end;
    char *interned_strings_top;
    apc_lck_t lock;
    HashTable interned_strings;
} apc_interned_strings_data_t;

apc_interned_strings_data_t *apc_interned_strings_data = NULL;

#define APCSG(v) (apc_interned_strings_data->v)

static char *old_interned_strings_start;
static char *old_interned_strings_end;
static const char *(*old_new_interned_string)(const char *str, int len, int free_src TSRMLS_DC);
static void (*old_interned_strings_snapshot)(TSRMLS_D);
static void (*old_interned_strings_restore)(TSRMLS_D);

static const char *apc_dummy_new_interned_string_for_php(const char *str, int len, int free_src TSRMLS_DC)
{
    return str;
}

static void apc_dummy_interned_strings_snapshot_for_php(TSRMLS_D)
{
}

static void apc_dummy_interned_strings_restore_for_php(TSRMLS_D)
{
}
#endif

const char *apc_new_interned_string(const char *arKey, int nKeyLength TSRMLS_DC)
{
#ifndef ZTS
    ulong h;
    uint nIndex;
    Bucket *p;

    if (!apc_interned_strings_data) {
        return NULL;
    }

    if (arKey >= APCSG(interned_strings_start) && arKey < APCSG(interned_strings_end)) {
        return arKey;
    }

    h = zend_inline_hash_func(arKey, nKeyLength);
    nIndex = h & APCSG(interned_strings).nTableMask;

    p = APCSG(interned_strings).arBuckets[nIndex];
    while (p != NULL) {
        if ((p->h == h) && (p->nKeyLength == nKeyLength)) {
            if (!memcmp(p->arKey, arKey, nKeyLength)) {
                return p->arKey;
            }
        }
        p = p->pNext;
    }
   
    if (APCSG(interned_strings_top) + ZEND_MM_ALIGNED_SIZE(sizeof(Bucket) + nKeyLength + 1) >=
        APCSG(interned_strings_end)) {
        /* no memory */
        return NULL;
    }

    p = (Bucket *) APCSG(interned_strings_top);
    APCSG(interned_strings_top) += ZEND_MM_ALIGNED_SIZE(sizeof(Bucket) + nKeyLength + 1);

    p->arKey = (char*)(p+1);
    memcpy((char*)p->arKey, arKey, nKeyLength);
    ((char *)p->arKey)[nKeyLength] = '\0';
    p->nKeyLength = nKeyLength;
    p->h = h;
    p->pData = &p->pDataPtr;
    p->pDataPtr = p;

    p->pNext = APCSG(interned_strings).arBuckets[nIndex];
    p->pLast = NULL;
    if (p->pNext) {
        p->pNext->pLast = p;
    }
    APCSG(interned_strings).arBuckets[nIndex] = p;

    p->pListLast = APCSG(interned_strings).pListTail;
    APCSG(interned_strings).pListTail = p;
    p->pListNext = NULL;
    if (p->pListLast != NULL) {
        p->pListLast->pListNext = p;
    }
    if (!APCSG(interned_strings).pListHead) {
        APCSG(interned_strings).pListHead = p;
    }

    APCSG(interned_strings).nNumOfElements++;

    return p->arKey;
#else
    return zend_new_interned_string(arKey, nKeyLength, 0 TSRMLS_CC);
#endif
}

#ifndef ZTS
static void apc_copy_internal_strings(TSRMLS_D)
{
    Bucket *p, *q;

    p = CG(function_table)->pListHead;
    while (p) {
        if (p->nKeyLength) {
            p->arKey = apc_new_interned_string(p->arKey, p->nKeyLength TSRMLS_CC);
        }
        p = p->pListNext;
    }

    p = CG(class_table)->pListHead;
    while (p) {
        zend_class_entry *ce = (zend_class_entry*)(p->pDataPtr);

        if (p->nKeyLength) {
            p->arKey = apc_new_interned_string(p->arKey, p->nKeyLength TSRMLS_CC);
        }

		if (ce->name) {
            ce->name = apc_new_interned_string(ce->name, ce->name_length+1 TSRMLS_CC);
		}

        q = ce->properties_info.pListHead;
        while (q) {
            zend_property_info *info = (zend_property_info*)(q->pData);

            if (q->nKeyLength) {
                q->arKey = apc_new_interned_string(q->arKey, q->nKeyLength TSRMLS_CC);
            }

            if (info->name) {
                info->name = apc_new_interned_string(info->name, info->name_length+1 TSRMLS_CC);
            }

            q = q->pListNext;
        }

        q = ce->function_table.pListHead;
        while (q) {
            if (q->nKeyLength) {
                q->arKey = apc_new_interned_string(q->arKey, q->nKeyLength TSRMLS_CC);
            }
            q = q->pListNext;
        }

        q = ce->constants_table.pListHead;
        while (q) {
            if (q->nKeyLength) {
                q->arKey = apc_new_interned_string(q->arKey, q->nKeyLength TSRMLS_CC);
            }
            q = q->pListNext;
        }

        p = p->pListNext;
    }

    p = EG(zend_constants)->pListHead;
    while (p) {
        if (p->nKeyLength) {
            p->arKey = apc_new_interned_string(p->arKey, p->nKeyLength TSRMLS_CC);
       }
        p = p->pListNext;
    }
}

void apc_interned_strings_init(TSRMLS_D)
{
    if (APCG(shm_strings_buffer) < APCG(shm_size)) {
        int count = APCG(shm_strings_buffer) / (sizeof(Bucket) + sizeof(Bucket*) * 2);

        apc_interned_strings_data = (apc_interned_strings_data_t*) apc_sma_malloc(APCG(shm_strings_buffer) TSRMLS_CC);
        if (apc_interned_strings_data) {
            memset((void *)apc_interned_strings_data, 0, APCG(shm_strings_buffer));

            CREATE_LOCK(APCSG(lock));

            zend_hash_init(&APCSG(interned_strings), count, NULL, NULL, 1);
            APCSG(interned_strings).nTableMask = APCSG(interned_strings).nTableSize - 1;
            APCSG(interned_strings).arBuckets = (Bucket**)((char*)apc_interned_strings_data + sizeof(apc_interned_strings_data_t));

            APCSG(interned_strings_start) = (char*)APCSG(interned_strings).arBuckets + APCSG(interned_strings).nTableSize * sizeof(Bucket *);
            APCSG(interned_strings_end)   = (char*)apc_interned_strings_data + APCG(shm_strings_buffer);
            APCSG(interned_strings_top)   = APCSG(interned_strings_start);

            old_interned_strings_start = CG(interned_strings_start);
            old_interned_strings_end = CG(interned_strings_end);
            old_new_interned_string = zend_new_interned_string;
            old_interned_strings_snapshot = zend_interned_strings_snapshot;
            old_interned_strings_restore = zend_interned_strings_restore;

            CG(interned_strings_start) = APCSG(interned_strings_start);
            CG(interned_strings_end) = APCSG(interned_strings_end);
            zend_new_interned_string = apc_dummy_new_interned_string_for_php;
            zend_interned_strings_snapshot = apc_dummy_interned_strings_snapshot_for_php;
            zend_interned_strings_restore = apc_dummy_interned_strings_restore_for_php;

            apc_copy_internal_strings(TSRMLS_C);
        } 
    } else {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "apc.shm_strings_buffer '%ld' exceed apc.shm_size '%ld'", APCG(shm_strings_buffer), APCG(shm_size));
    }
}

void apc_interned_strings_shutdown(TSRMLS_D)
{	
    if (apc_interned_strings_data) {
        zend_hash_clean(CG(function_table));
        zend_hash_clean(CG(class_table));
        zend_hash_clean(EG(zend_constants));

        CG(interned_strings_start) = old_interned_strings_start;
        CG(interned_strings_end) = old_interned_strings_end;
        zend_new_interned_string = old_new_interned_string;
        zend_interned_strings_snapshot = old_interned_strings_snapshot;
        zend_interned_strings_restore = old_interned_strings_restore;

        DESTROY_LOCK(APCSG(lock));
    }
}
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
