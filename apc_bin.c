/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt.                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Brian Shire <shire@php.net>                                 |
  |          Xinchen Hui <laruence@php.net>                              |
  +----------------------------------------------------------------------+

 */

/* $Id: apc_bin.c 328828 2012-12-18 15:18:39Z remi $ */

/* Creates a binary architecture specific output to a string or file containing
 * the current cache contents for both fies and user variables.  This is accomplished
 * via the apc_copy_* functions and "swizzling" pointer values to a position
 * independent value, and unswizzling them on restoration.
 */

#include "apc_globals.h"
#include "apc_bin.h"
#include "apc_zend.h"
#include "apc_php.h"
#include "apc_sma.h"
#include "apc_pool.h"
#include "ext/standard/md5.h"

extern apc_cache_t* apc_cache;
extern apc_cache_t* apc_user_cache;

extern int _apc_store(char *strkey, int strkey_len, const zval *val, const uint ttl, const int exclusive TSRMLS_DC); /* this is hacky */

#define APC_BINDUMP_DEBUG 0

#if APC_BINDUMP_DEBUG

#define SWIZZLE(bd, ptr)  \
    do { \
        if((long)bd < (long)ptr && (ulong)ptr < ((long)bd + bd->size)) { \
            printf("SWIZZLE: %x ~> ", ptr); \
            ptr = (void*)((long)(ptr) - (long)(bd)); \
            printf("%x in %s on line %d", ptr, __FILE__, __LINE__); \
        } else if((long)ptr > bd->size) { /* not swizzled */ \
            apc_error("pointer to be swizzled is not within allowed memory range! (%x < %x < %x) in %s on %d" TSRMLS_CC, (long)bd, ptr, ((long)bd + bd->size), __FILE__, __LINE__); \
            return; \
        } \
        printf("\n"); \
    } while(0);

#define UNSWIZZLE(bd, ptr)  \
    do { \
      printf("UNSWIZZLE: %x -> ", ptr); \
      ptr = (void*)((long)(ptr) + (long)(bd)); \
      printf("%x in %s on line %d \n", ptr, __FILE__, __LINE__); \
    } while(0);

#else    /* !APC_BINDUMP_DEBUG */

#define SWIZZLE(bd, ptr) \
    do { \
        if((long)bd < (long)ptr && (ulong)ptr < ((long)bd + bd->size)) { \
            ptr = (void*)((long)(ptr) - (long)(bd)); \
        } else if((ulong)ptr > bd->size) { /* not swizzled */ \
            apc_error("pointer to be swizzled is not within allowed memory range! (%x < %x < %x) in %s on %d" TSRMLS_CC, (long)bd, ptr, ((long)bd + bd->size), __FILE__, __LINE__); \
            return NULL; \
        } \
    } while(0);

#define UNSWIZZLE(bd, ptr) \
    do { \
      ptr = (void*)((long)(ptr) + (long)(bd)); \
    } while(0);

#endif

static void *apc_bd_alloc(size_t size TSRMLS_DC);
static void apc_bd_free(void *ptr TSRMLS_DC);
static void *apc_bd_alloc_ex(void *ptr_new, size_t size TSRMLS_DC);

typedef void (*apc_swizzle_cb_t)(apc_bd_t *bd, zend_llist *ll, void *ptr TSRMLS_DC);

#if APC_BINDUMP_DEBUG
#define apc_swizzle_ptr(bd, ll, ptr) _apc_swizzle_ptr(bd, ll, (void*)ptr, __FILE__, __LINE__ TSRMLS_CC)
#else
#define apc_swizzle_ptr(bd, ll, ptr) _apc_swizzle_ptr(bd, ll, (void*)ptr, NULL, 0 TSRMLS_CC)
#endif

static void _apc_swizzle_ptr(apc_bd_t *bd, zend_llist *ll, void **ptr, const char* file, int line TSRMLS_DC);
static void apc_swizzle_function(apc_bd_t *bd, zend_llist *ll, zend_function *func TSRMLS_DC);
static void apc_swizzle_class_entry(apc_bd_t *bd, zend_llist *ll, zend_class_entry *ce TSRMLS_DC);
static void apc_swizzle_hashtable(apc_bd_t *bd, zend_llist *ll, HashTable *ht, apc_swizzle_cb_t swizzle_cb, int is_ptr TSRMLS_DC);
static void apc_swizzle_zval(apc_bd_t *bd, zend_llist *ll, zval *zv TSRMLS_DC);
static void apc_swizzle_op_array(apc_bd_t *bd, zend_llist *ll, zend_op_array *op_array TSRMLS_DC);
static void apc_swizzle_property_info(apc_bd_t *bd, zend_llist *ll, zend_property_info *pi TSRMLS_DC);
static void apc_swizzle_function_entry(apc_bd_t *bd, zend_llist *ll, const zend_function_entry *fe TSRMLS_DC);
static void apc_swizzle_arg_info_array(apc_bd_t *bd, zend_llist *ll, const zend_arg_info* arg_info_array, uint num_args TSRMLS_DC);

static apc_bd_t* apc_swizzle_bd(apc_bd_t* bd, zend_llist *ll TSRMLS_DC);
static int apc_unswizzle_bd(apc_bd_t *bd, int flags TSRMLS_DC);

/* {{{ apc_bd_alloc
 *  callback for copy_* functions */
static void *apc_bd_alloc(size_t size TSRMLS_DC) {
    return apc_bd_alloc_ex(NULL, size TSRMLS_CC);
} /* }}} */

/* {{{ apc_bd_free
 *  callback for copy_* functions */
static void apc_bd_free(void *ptr TSRMLS_DC) {
    size_t *size;
    if(zend_hash_index_find(&APCG(apc_bd_alloc_list), (ulong)ptr, (void**)&size) == FAILURE) {
        apc_error("apc_bd_free could not free pointer (not found in list: %x)" TSRMLS_CC, ptr);
        return;
    }
    APCG(apc_bd_alloc_ptr) = (void*)((size_t)APCG(apc_bd_alloc_ptr) - *size);
    zend_hash_index_del(&APCG(apc_bd_alloc_list), (ulong)ptr);
} /* }}} */

/* {{{ apc_bd_alloc_ex
 *  set ranges or allocate a block of data from an already (e)malloc'd range.
 *  if ptr_new is not NULL, it will reset the pointer to start at ptr_new,
 *  with a range of size.  If ptr_new is NULL, returns the next available
 *  block of given size.
 */
static void *apc_bd_alloc_ex(void *ptr_new, size_t size TSRMLS_DC) {
    void *rval;

    rval = APCG(apc_bd_alloc_ptr);
    if(ptr_new != NULL) {  /* reset ptrs */
      APCG(apc_bd_alloc_ptr) = ptr_new;
      APCG(apc_bd_alloc_ubptr) = (void*)((unsigned char *) ptr_new + size);
    } else {  /* alloc block */
      APCG(apc_bd_alloc_ptr) = (void*)((size_t)APCG(apc_bd_alloc_ptr) + size);
#if APC_BINDUMP_DEBUG
      apc_notice("apc_bd_alloc: rval: 0x%x  ptr: 0x%x  ubptr: 0x%x  size: %d" TSRMLS_CC, rval, APCG(apc_bd_alloc_ptr), APCG(apc_bd_alloc_ubptr), size);
#endif
      if(APCG(apc_bd_alloc_ptr) > APCG(apc_bd_alloc_ubptr)) {
          apc_error("Exceeded bounds check in apc_bd_alloc_ex by %d bytes." TSRMLS_CC, (unsigned char *) APCG(apc_bd_alloc_ptr) - (unsigned char *) APCG(apc_bd_alloc_ubptr));
          return NULL;
      }
      zend_hash_index_update(&APCG(apc_bd_alloc_list), (ulong)rval, &size, sizeof(size_t), NULL);
    }

    return rval;
} /* }}} */

/* {{{ _apc_swizzle_ptr */
static void _apc_swizzle_ptr(apc_bd_t *bd, zend_llist *ll, void **ptr, const char* file, int line TSRMLS_DC) {
    if(*ptr) {
        if((long)bd < (long)*ptr && (ulong)*ptr < ((long)bd + bd->size)) {
            zend_llist_add_element(ll, &ptr);
#if APC_BINDUMP_DEBUG
            printf("[%06d] apc_swizzle_ptr: %x -> %x ", zend_llist_count(ll), ptr, *ptr);
            printf(" in %s on line %d \n", file, line);
#endif
        } else if((ulong)ptr > bd->size) {
            apc_error("pointer to be swizzled is not within allowed memory range! (%x < %x < %x) in %s on %d" TSRMLS_CC, (long)bd, *ptr, ((long)bd + bd->size), file, line); \
            return;
        }
    }
} /* }}} */

/* {{{ apc_swizzle_op_array */
static void apc_swizzle_op_array(apc_bd_t *bd, zend_llist *ll, zend_op_array *op_array TSRMLS_DC) {
    uint i;

#ifdef ZEND_ENGINE_2
    apc_swizzle_arg_info_array(bd, ll, op_array->arg_info, op_array->num_args TSRMLS_CC);
    apc_swizzle_ptr(bd, ll, &op_array->arg_info);
#else
    if (op_array->arg_types) {
        apc_swizzle_ptr(bd, ll, &op_array->arg_types);
    }
#endif

    apc_swizzle_ptr(bd, ll, &op_array->function_name);
    apc_swizzle_ptr(bd, ll, &op_array->filename);
    apc_swizzle_ptr(bd, ll, &op_array->refcount);
#ifdef ZEND_ENGINE_2_4
    if (op_array->last_literal) {
        int j = 0;
        apc_swizzle_ptr(bd, ll, &(op_array->literals));
        for (; j<op_array->last_literal; j++) {
            apc_swizzle_zval(bd, ll, &((op_array->literals[j]).constant) TSRMLS_CC);
        }
    }
#endif

    /* swizzle op_array */
    for(i=0; i < op_array->last; i++) {
#ifndef ZEND_ENGINE_2_4
        if(op_array->opcodes[i].result.op_type == IS_CONST) {
            apc_swizzle_zval(bd, ll, &op_array->opcodes[i].result.u.constant TSRMLS_CC);
        }
        if(op_array->opcodes[i].op1.op_type == IS_CONST) {
            apc_swizzle_zval(bd, ll, &op_array->opcodes[i].op1.u.constant TSRMLS_CC);
        }
        if(op_array->opcodes[i].op2.op_type == IS_CONST) {
            apc_swizzle_zval(bd, ll, &op_array->opcodes[i].op2.u.constant TSRMLS_CC);
        }
#else
        if (op_array->opcodes[i].op1_type == IS_CONST) {
            apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].op1.literal);
        }
        if (op_array->opcodes[i].op2_type == IS_CONST) {
            apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].op2.literal);
        }
        if (op_array->opcodes[i].result_type == IS_CONST) {
            apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].result.literal);
        }
#endif
        switch (op_array->opcodes[i].opcode) {
#ifdef ZEND_ENGINE_2_3
            case ZEND_GOTO:
#endif
            case ZEND_JMP:
#ifdef ZEND_ENGINE_2_4
                apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].op1.jmp_addr);
#else
                apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].op1.u.jmp_addr);
#endif
                break;
            case ZEND_JMPZ:
            case ZEND_JMPNZ:
            case ZEND_JMPZ_EX:
            case ZEND_JMPNZ_EX:
#ifdef ZEND_ENGINE_2_3
            case ZEND_JMP_SET:
#endif
#ifdef ZEND_ENGINE_2_4
            case ZEND_JMP_SET_VAR:
#endif
#ifdef ZEND_ENGINE_2_4
                apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].op2.jmp_addr);
#else
                apc_swizzle_ptr(bd, ll, &op_array->opcodes[i].op2.u.jmp_addr);
#endif
        }
    }
    apc_swizzle_ptr(bd, ll, &op_array->opcodes);

    /* break-continue array ptr */
    if(op_array->brk_cont_array) {
        apc_swizzle_ptr(bd, ll, &op_array->brk_cont_array);
    }

    /* static voriables */
    if(op_array->static_variables) {
        apc_swizzle_hashtable(bd, ll, op_array->static_variables, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);
        apc_swizzle_ptr(bd, ll, &op_array->static_variables);
    }

#ifdef ZEND_ENGINE_2
    /* try-catch */
    if(op_array->try_catch_array) {
        apc_swizzle_ptr(bd, ll, &op_array->try_catch_array);
    }
#endif

#ifdef ZEND_ENGINE_2_1 /* PHP 5.1 */
    /* vars */
    if(op_array->vars) {
        for(i=0; (signed int) i < op_array->last_var; i++) {
            apc_swizzle_ptr(bd, ll, &op_array->vars[i].name);
        }
        apc_swizzle_ptr(bd, ll, &op_array->vars);
    }
#endif

#ifdef ZEND_ENGINE_2
    /* doc comment */
    if(op_array->doc_comment) {
        apc_swizzle_ptr(bd, ll, &op_array->doc_comment);
    }
#endif

} /* }}} */

/* {{{ apc_swizzle_function */
static void apc_swizzle_function(apc_bd_t *bd, zend_llist *ll, zend_function *func TSRMLS_DC) {
    apc_swizzle_op_array(bd, ll, &func->op_array TSRMLS_CC);
#ifdef ZEND_ENGINE_2
    if(func->common.scope) {
        apc_swizzle_ptr(bd, ll, &func->common.scope);
    }
#endif
} /* }}} */

/* {{{ apc_swizzle_class_entry */
static void apc_swizzle_class_entry(apc_bd_t *bd, zend_llist *ll, zend_class_entry *ce TSRMLS_DC) {

    uint i;

    if(ce->name) {
        apc_swizzle_ptr(bd, ll, &ce->name);
    }

    if (ce->type == ZEND_USER_CLASS && ZEND_CE_DOC_COMMENT(ce)) {
        apc_swizzle_ptr(bd, ll, &ZEND_CE_DOC_COMMENT(ce));
    }

#ifndef ZEND_ENGINE_2
    apc_swizzle_ptr(bd, ll, &ce->refcount);
#endif

    apc_swizzle_hashtable(bd, ll, &ce->function_table, (apc_swizzle_cb_t)apc_swizzle_function, 0 TSRMLS_CC);
#ifdef ZEND_ENGINE_2_4
    if (ce->default_properties_table) {
        apc_swizzle_ptr(bd, ll, &ce->default_properties_table);
        for (i = 0; i < ce->default_properties_count; i++) {
            if (ce->default_properties_table[i]) {
                apc_swizzle_ptr(bd, ll, &ce->default_properties_table[i]);
                apc_swizzle_zval(bd, ll, ce->default_properties_table[i] TSRMLS_CC);
            }
        }
    }
#else
    apc_swizzle_hashtable(bd, ll, &ce->default_properties, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);
#endif

#ifdef ZEND_ENGINE_2
    apc_swizzle_hashtable(bd, ll, &ce->properties_info, (apc_swizzle_cb_t)apc_swizzle_property_info, 0 TSRMLS_CC);
#endif

#ifdef ZEND_ENGINE_2_4
    if (ce->default_static_members_table) {
        apc_swizzle_ptr(bd, ll, &ce->default_static_members_table);
        for (i = 0; i < ce->default_static_members_count; i++) {
            if (ce->default_static_members_table[i]) {
                apc_swizzle_ptr(bd, ll, &ce->default_static_members_table[i]);
                apc_swizzle_zval(bd, ll, ce->default_static_members_table[i] TSRMLS_CC);
            }
        }
    }
    ce->static_members_table = ce->default_static_members_table;
#else
    apc_swizzle_hashtable(bd, ll, &ce->default_static_members, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);

    if(ce->static_members != &ce->default_static_members) {
        apc_swizzle_hashtable(bd, ll, ce->static_members, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);
    } else {
        apc_swizzle_ptr(bd, ll, &ce->static_members);
    }
#endif

    apc_swizzle_hashtable(bd, ll, &ce->constants_table, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);

    if(ce->type == ZEND_INTERNAL_CLASS &&  ZEND_CE_BUILTIN_FUNCTIONS(ce)) {
        for(i=0; ZEND_CE_BUILTIN_FUNCTIONS(ce)[i].fname; i++) {
            apc_swizzle_function_entry(bd, ll, &ZEND_CE_BUILTIN_FUNCTIONS(ce)[i] TSRMLS_CC);
        }
    }

    apc_swizzle_ptr(bd, ll, &ce->constructor);
    apc_swizzle_ptr(bd, ll, &ce->destructor);
    apc_swizzle_ptr(bd, ll, &ce->clone);
    apc_swizzle_ptr(bd, ll, &ce->__get);
    apc_swizzle_ptr(bd, ll, &ce->__set);
    apc_swizzle_ptr(bd, ll, &ce->__unset);
    apc_swizzle_ptr(bd, ll, &ce->__isset);
    apc_swizzle_ptr(bd, ll, &ce->__call);
    apc_swizzle_ptr(bd, ll, &ce->serialize_func);
    apc_swizzle_ptr(bd, ll, &ce->unserialize_func);

#ifdef ZEND_ENGINE_2_2
    apc_swizzle_ptr(bd, ll, &ce->__tostring);
#endif

    if (ce->type == ZEND_USER_CLASS) {
        apc_swizzle_ptr(bd, ll, &ZEND_CE_FILENAME(ce));
    }
} /* }}} */

/* {{{ apc_swizzle_property_info */
static void apc_swizzle_property_info(apc_bd_t *bd, zend_llist *ll, zend_property_info *pi TSRMLS_DC) {
    apc_swizzle_ptr(bd, ll, &pi->name);
    apc_swizzle_ptr(bd, ll, &pi->doc_comment);

#ifdef ZEND_ENGINE_2_2
    apc_swizzle_ptr(bd, ll, &pi->ce);
#endif
} /* }}} */

/* {{{ apc_swizzle_function_entry */
static void apc_swizzle_function_entry(apc_bd_t *bd, zend_llist *ll, const zend_function_entry *fe TSRMLS_DC) {
    apc_swizzle_ptr(bd, ll, &fe->fname);
    apc_swizzle_arg_info_array(bd, ll, fe->arg_info, fe->num_args TSRMLS_CC);
    apc_swizzle_ptr(bd, ll, &fe->arg_info);
} /* }}} */

/* {{{ apc_swizzle_arg_info_array */
static void apc_swizzle_arg_info_array(apc_bd_t *bd, zend_llist *ll, const zend_arg_info* arg_info_array, uint num_args TSRMLS_DC) {
    if(arg_info_array) {
        uint i;

        for(i=0; i < num_args; i++) {
            apc_swizzle_ptr(bd, ll, &arg_info_array[i].name);
            apc_swizzle_ptr(bd, ll, &arg_info_array[i].class_name);
        }
    }

} /* }}} */

/* {{{ apc_swizzle_hashtable */
static void apc_swizzle_hashtable(apc_bd_t *bd, zend_llist *ll, HashTable *ht, apc_swizzle_cb_t swizzle_cb, int is_ptr TSRMLS_DC) {
    uint i;
    Bucket **bp, **bp_prev;

    bp = &ht->pListHead;
    while(*bp) {
        bp_prev = bp;
        bp = &(*bp)->pListNext;
        if(is_ptr) {
            swizzle_cb(bd, ll, *(void**)(*bp_prev)->pData TSRMLS_CC);
            apc_swizzle_ptr(bd, ll, (*bp_prev)->pData);
        } else {
            swizzle_cb(bd, ll, (void**)(*bp_prev)->pData TSRMLS_CC);
        }
#ifdef ZEND_ENGINE_2_4
        if ((*bp_prev)->nKeyLength) {
            if (IS_INTERNED((*bp_prev)->arKey)) {
                /* we should dump that internal string out */
                char *tmp = apc_bd_alloc((*bp_prev)->nKeyLength TSRMLS_CC);
                memcpy(tmp, (*bp_prev)->arKey, (*bp_prev)->nKeyLength);
                (*bp_prev)->arKey = tmp;
            }
            apc_swizzle_ptr(bd, ll, &(*bp_prev)->arKey);
        }
#endif
        apc_swizzle_ptr(bd, ll, &(*bp_prev)->pData);
        if((*bp_prev)->pDataPtr) {
            apc_swizzle_ptr(bd, ll, &(*bp_prev)->pDataPtr);
        }
        if((*bp_prev)->pListLast) {
            apc_swizzle_ptr(bd, ll, &(*bp_prev)->pListLast);
        }
        if((*bp_prev)->pNext) {
            apc_swizzle_ptr(bd, ll, &(*bp_prev)->pNext);
        }
        if((*bp_prev)->pLast) {
            apc_swizzle_ptr(bd, ll, &(*bp_prev)->pLast);
        }
        apc_swizzle_ptr(bd, ll, bp_prev);
    }
    for(i=0; i < ht->nTableSize; i++) {
        if(ht->arBuckets[i]) {
            apc_swizzle_ptr(bd, ll, &ht->arBuckets[i]);
        }
    }
    apc_swizzle_ptr(bd, ll, &ht->pListTail);

    apc_swizzle_ptr(bd, ll, &ht->arBuckets);
} /* }}} */

/* {{{ apc_swizzle_zval */
static void apc_swizzle_zval(apc_bd_t *bd, zend_llist *ll, zval *zv TSRMLS_DC) {

    if(APCG(copied_zvals).nTableSize) {
        if(zend_hash_index_exists(&APCG(copied_zvals), (ulong)zv)) {
          return;
        }
        zend_hash_index_update(&APCG(copied_zvals), (ulong)zv, (void**)&zv, sizeof(zval*), NULL);
    }

    switch(Z_TYPE_P(zv) & IS_CONSTANT_TYPE_MASK) {
        case IS_NULL:
        case IS_LONG:
        case IS_DOUBLE:
        case IS_BOOL:
        case IS_RESOURCE:
            /* nothing to do */
            break;
        case IS_CONSTANT:
        case IS_STRING:
            apc_swizzle_ptr(bd, ll, &zv->value.str.val);
            break;
        case IS_ARRAY:
        case IS_CONSTANT_ARRAY:
            apc_swizzle_hashtable(bd, ll, zv->value.ht, (apc_swizzle_cb_t)apc_swizzle_zval, 1 TSRMLS_CC);
            apc_swizzle_ptr(bd, ll, &zv->value.ht);
            break;
        case IS_OBJECT:
            break;
        default:
            assert(0); /* shouldn't happen */
    }
} /* }}} */

/* {{{ apc_swizzle_bd */
static apc_bd_t* apc_swizzle_bd(apc_bd_t* bd, zend_llist *ll TSRMLS_DC) {
    unsigned int i;
    int count;
    PHP_MD5_CTX context;
    unsigned char digest[16];
    register php_uint32 crc;
    void ***ptr;
    void ***ptr_list;

    count = zend_llist_count(ll);
    ptr_list = emalloc(count * sizeof(void**));
    ptr = zend_llist_get_first(ll);
    for(i=0; i < count; i++) {
#if APC_BINDUMP_DEBUG
        printf("[%06d] ", i+1);
#endif
        SWIZZLE(bd, **ptr); /* swizzle ptr */
        if((long)bd < (long)*ptr && (ulong)*ptr < ((long)bd + bd->size)) {  /* exclude ptrs that aren't actually included in the ptr list */
#if APC_BINDUMP_DEBUG
            printf("[------] ");
#endif
            SWIZZLE(bd, *ptr);  /* swizzle ptr list */
            ptr_list[i] = *ptr;
        }
        ptr = zend_llist_get_next(ll);
    }
    SWIZZLE(bd, bd->entries);

    if(count > 0) {
        bd = erealloc(bd, bd->size + (count * sizeof(void**)));
        bd->num_swizzled_ptrs = count;
        bd->swizzled_ptrs = (void***)((unsigned char *)bd + bd->size -2);   /* extra -1 for null termination */
        bd->size += count * sizeof(void**);
        memcpy(bd->swizzled_ptrs, ptr_list, count * sizeof(void**));
        SWIZZLE(bd, bd->swizzled_ptrs);
    } else {
        bd->num_swizzled_ptrs = 0;
        bd->swizzled_ptrs = NULL;
    }
    ((unsigned char*)bd)[((bd->size >= 1) ? (bd->size-1) : 0)] = 0;  /* silence null termination for zval strings */
    efree(ptr_list);
    bd->swizzled = 1;

    /* Generate MD5/CRC32 checksum */
    memset(bd->md5, 0, 16);
    bd->crc=0;

    PHP_MD5Init(&context);
    PHP_MD5Update(&context, (const unsigned char*)bd, bd->size);
    PHP_MD5Final(digest, &context);
    crc = apc_crc32((unsigned char*)bd, bd->size);

    memmove(bd->md5, digest, 16);
    bd->crc = crc;

    return bd;
} /* }}} */

/* {{{ apc_unswizzle_bd */
static int apc_unswizzle_bd(apc_bd_t *bd, int flags TSRMLS_DC) {
    unsigned int i;
    unsigned char md5_orig[16];
    unsigned char digest[16];
    PHP_MD5_CTX context;
    register php_uint32 crc;
    php_uint32 crc_orig;

    /* Verify the md5 or crc32 before we unswizzle */
    memmove(md5_orig, bd->md5, 16);
    memset(bd->md5, 0, 16);
    crc_orig = bd->crc;
    bd->crc=0;

    if(flags & APC_BIN_VERIFY_MD5) {
        PHP_MD5Init(&context);
        PHP_MD5Update(&context, (const unsigned char*)bd, bd->size);
        PHP_MD5Final(digest, &context);
        if(memcmp(md5_orig, digest, 16)) {
            apc_error("MD5 checksum of binary dump failed." TSRMLS_CC);
            return -1;
        }
    }
    if(flags & APC_BIN_VERIFY_CRC32) {
        crc = apc_crc32((unsigned char*)bd, bd->size);
        if(crc_orig != crc) {
            apc_error("CRC32 checksum of binary dump failed." TSRMLS_CC);
            return -1;
        }
    }
    memcpy(bd->md5, md5_orig, 16); /* add back md5 checksum */
    bd->crc = crc_orig;

    UNSWIZZLE(bd, bd->entries);
    UNSWIZZLE(bd, bd->swizzled_ptrs);
    for(i=0; i < bd->num_swizzled_ptrs; i++) {
        if(bd->swizzled_ptrs[i]) {
            UNSWIZZLE(bd, bd->swizzled_ptrs[i]);
            if(*bd->swizzled_ptrs[i] && (*bd->swizzled_ptrs[i] < (void*)bd)) {
                UNSWIZZLE(bd, *bd->swizzled_ptrs[i]);
            }
        }
    }

    bd->swizzled=0;

    return 0;
} /* }}} */

/* {{{ apc_bin_checkfilter */
static int apc_bin_checkfilter(HashTable *filter, const char *key, uint key_len) {
    zval **zptr;

    if(filter == NULL) {
        return 1;
    }

    if(zend_hash_find(filter, (char*)key, key_len, (void**)&zptr) == SUCCESS) {
        if(Z_TYPE_PP(zptr) == IS_LONG && Z_LVAL_PP(zptr) == 0) {
            return 0;
        }
    } else {
        return 0;
    }


    return 1;
} /* }}} */

/* {{{ apc_bin_fixup_op_array */
static inline void apc_bin_fixup_op_array(zend_op_array *op_array) {
    ulong i;
    for (i = 0; i < op_array->last; i++) {
        op_array->opcodes[i].handler = zend_opcode_handlers[APC_OPCODE_HANDLER_DECODE(&op_array->opcodes[i])];
    }
}
/* }}} */

/* {{{ apc_bin_fixup_class_entry */
static inline void apc_bin_fixup_class_entry(zend_class_entry *ce) {
    zend_function *fe;
    HashPosition hpos;

    /* fixup the opcodes in each method */
    zend_hash_internal_pointer_reset_ex(&ce->function_table, &hpos);
    while(zend_hash_get_current_data_ex(&ce->function_table, (void**)&fe, &hpos) == SUCCESS) {
        apc_bin_fixup_op_array(&fe->op_array);
        zend_hash_move_forward_ex(&ce->function_table, &hpos);
    }

    /* fixup hashtable destructor pointers */
    ce->function_table.pDestructor = (dtor_func_t)zend_function_dtor;
#ifndef ZEND_ENGINE_2_4
    ce->default_properties.pDestructor = (dtor_func_t)zval_ptr_dtor_wrapper;
#endif
    ce->properties_info.pDestructor = (dtor_func_t)zval_ptr_dtor_wrapper;
#ifndef ZEND_ENGINE_2_4
    ce->default_static_members.pDestructor = (dtor_func_t)zval_ptr_dtor_wrapper;
    if (ce->static_members) {
        ce->static_members->pDestructor = (dtor_func_t)zval_ptr_dtor_wrapper;
    }
#endif
    ce->constants_table.pDestructor = (dtor_func_t)zval_ptr_dtor_wrapper;
}
/* }}} */

/* {{{ apc_bin_dump */
apc_bd_t* apc_bin_dump(HashTable *files, HashTable *user_vars TSRMLS_DC) {
    uint fcount;
    slot_t *sp;
    apc_bd_entry_t *ep;
    int i, count=0;
    apc_bd_t *bd;
    zend_llist ll;
    zend_function *efp, *sfp;
    size_t size=0;
    apc_context_t ctxt;
    void *pool_ptr;

    zend_llist_init(&ll, sizeof(void*), NULL, 0);
    zend_hash_init(&APCG(apc_bd_alloc_list), 0, NULL, NULL, 0);

    /* flip the hash for faster filter checking */
    files = apc_flip_hash(files);
    user_vars = apc_flip_hash(user_vars);

    /* get size and entry counts */
    for(i=0; i < apc_user_cache->num_slots; i++) {
        sp = apc_user_cache->slots[i];
        for(; sp != NULL; sp = sp->next) {
            if(apc_bin_checkfilter(user_vars, sp->key.data.user.identifier, sp->key.data.user.identifier_len)) {
                size += sizeof(apc_bd_entry_t*) + sizeof(apc_bd_entry_t);
                size += sp->value->mem_size - (sizeof(apc_cache_entry_t) - sizeof(apc_cache_entry_value_t));
                count++;
            }
        }
    }
    for(i=0; i < apc_cache->num_slots; i++) {
        sp = apc_cache->slots[i];
        for(; sp != NULL; sp = sp->next) {
            if(sp->key.type == APC_CACHE_KEY_FPFILE) {
                if(apc_bin_checkfilter(files, sp->key.data.fpfile.fullpath, sp->key.data.fpfile.fullpath_len+1)) {
                    size += sizeof(apc_bd_entry_t*) + sizeof(apc_bd_entry_t);
                    size += sp->value->mem_size - (sizeof(apc_cache_entry_t) - sizeof(apc_cache_entry_value_t));
                    count++;
                }
            } else {
                /* TODO: Currently we don't support APC_CACHE_KEY_FILE type.  We need to store the path and re-stat on load */
                apc_warning("Excluding some files from apc_bin_dump[file].  Cached files must be included using full path with apc.stat=0." TSRMLS_CC);
            }
        }
    }

    size += sizeof(apc_bd_t) +1;  /* +1 for null termination */
    bd = emalloc(size);
    bd->size = (unsigned int)size;
    pool_ptr = emalloc(sizeof(apc_pool));
    apc_bd_alloc_ex(pool_ptr, sizeof(apc_pool) TSRMLS_CC);
    ctxt.pool = apc_pool_create(APC_UNPOOL, apc_bd_alloc, apc_bd_free, NULL, NULL TSRMLS_CC);  /* ideally the pool wouldn't be alloc'd as part of this */
    if (!ctxt.pool) { /* TODO need to cleanup */
        apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
        return NULL;
    }
    ctxt.copy = APC_COPY_IN_OPCODE; /* avoid stupid ALLOC_ZVAL calls here, hack */
    apc_bd_alloc_ex((void*)((long)bd + sizeof(apc_bd_t)), bd->size - sizeof(apc_bd_t) -1 TSRMLS_CC);
    bd->num_entries = count;
    bd->entries = apc_bd_alloc_ex(NULL, sizeof(apc_bd_entry_t) * count TSRMLS_CC);

    /* User entries */
    zend_hash_init(&APCG(copied_zvals), 0, NULL, NULL, 0);
    count = 0;
    for(i=0; i < apc_user_cache->num_slots; i++) {
        sp = apc_user_cache->slots[i];
        for(; sp != NULL; sp = sp->next) {
            if(apc_bin_checkfilter(user_vars, sp->key.data.user.identifier, sp->key.data.user.identifier_len)) {
                ep = &bd->entries[count];
                ep->type = sp->value->type;
                ep->val.user.info = apc_bd_alloc(sp->value->data.user.info_len TSRMLS_CC);
                memcpy(ep->val.user.info, sp->value->data.user.info, sp->value->data.user.info_len);
                ep->val.user.info_len = sp->value->data.user.info_len;
                if ((Z_TYPE_P(sp->value->data.user.val) == IS_ARRAY && APCG(serializer))
                        || Z_TYPE_P(sp->value->data.user.val) == IS_OBJECT) {
                    /* avoiding hash copy, hack */
                    uint type = Z_TYPE_P(sp->value->data.user.val);
                    Z_TYPE_P(sp->value->data.user.val) = IS_STRING;
                    ep->val.user.val = apc_copy_zval(NULL, sp->value->data.user.val, &ctxt TSRMLS_CC);
                    Z_TYPE_P(ep->val.user.val) = IS_OBJECT;
                    sp->value->data.user.val->type = type;
                } else if (Z_TYPE_P(sp->value->data.user.val) == IS_ARRAY && !APCG(serializer)) {
                    /* this is a little complicated, we have to unserialize it first, then serialize it again */
                    zval *garbage;
                    ctxt.copy = APC_COPY_OUT_USER;
                    garbage = apc_copy_zval(NULL, sp->value->data.user.val, &ctxt TSRMLS_CC);
                    APCG(serializer) = apc_find_serializer("php" TSRMLS_CC);
                    ctxt.copy = APC_COPY_IN_USER;
                    ep->val.user.val = apc_copy_zval(NULL, garbage, &ctxt TSRMLS_CC);
                    ep->val.user.val->type = IS_OBJECT;
                    /* a memleak can not be avoided: zval_ptr_dtor(&garbage); */
                    APCG(serializer) = NULL;
                    ctxt.copy = APC_COPY_IN_OPCODE;
                } else {
                    ep->val.user.val = apc_copy_zval(NULL, sp->value->data.user.val, &ctxt TSRMLS_CC);
                }
                ep->val.user.ttl = sp->value->data.user.ttl;

                /* swizzle pointers */
                apc_swizzle_ptr(bd, &ll, &bd->entries[count].val.user.info);
                zend_hash_clean(&APCG(copied_zvals));
                if (ep->val.user.val->type == IS_OBJECT) {
                    apc_swizzle_ptr(bd, &ll, &bd->entries[count].val.user.val->value.str.val);
                } else {
                    apc_swizzle_zval(bd, &ll, bd->entries[count].val.user.val TSRMLS_CC);
                }
                apc_swizzle_ptr(bd, &ll, &bd->entries[count].val.user.val);

                count++;
            }
        }
    }
    zend_hash_destroy(&APCG(copied_zvals));
    APCG(copied_zvals).nTableSize=0;

    /* File entries */
    for(i=0; i < apc_cache->num_slots; i++) {
        for(sp=apc_cache->slots[i]; sp != NULL; sp = sp->next) {
            if(sp->key.type == APC_CACHE_KEY_FPFILE) {
                if(apc_bin_checkfilter(files, sp->key.data.fpfile.fullpath, sp->key.data.fpfile.fullpath_len+1)) {
                    ep = &bd->entries[count];
                    ep->type = sp->key.type;
                    memmove(ep->file_md5, sp->key.md5, 16);
                    ep->val.file.filename = apc_bd_alloc(strlen(sp->value->data.file.filename) + 1 TSRMLS_CC);
                    strcpy(ep->val.file.filename, sp->value->data.file.filename);
                    ep->val.file.op_array = apc_copy_op_array(NULL, sp->value->data.file.op_array, &ctxt TSRMLS_CC);

                    for(ep->num_functions=0; sp->value->data.file.functions[ep->num_functions].function != NULL;) { ep->num_functions++; }
                    ep->val.file.functions = apc_bd_alloc(sizeof(apc_function_t) * ep->num_functions TSRMLS_CC);
                    for(fcount=0; fcount < ep->num_functions; fcount++) {
                        memcpy(&ep->val.file.functions[fcount], &sp->value->data.file.functions[fcount], sizeof(apc_function_t));
                        ep->val.file.functions[fcount].name = apc_xmemcpy(sp->value->data.file.functions[fcount].name, sp->value->data.file.functions[fcount].name_len+1, apc_bd_alloc TSRMLS_CC);
                        ep->val.file.functions[fcount].name_len = sp->value->data.file.functions[fcount].name_len;
                        ep->val.file.functions[fcount].function = apc_bd_alloc(sizeof(zend_function) TSRMLS_CC);
                        efp = ep->val.file.functions[fcount].function;
                        sfp = sp->value->data.file.functions[fcount].function;
                        switch(sfp->type) {
                            case ZEND_INTERNAL_FUNCTION:
                            case ZEND_OVERLOADED_FUNCTION:
                                efp->op_array = sfp->op_array;
                                break;
                            case ZEND_USER_FUNCTION:
                            case ZEND_EVAL_CODE:
                                apc_copy_op_array(&efp->op_array, &sfp->op_array, &ctxt TSRMLS_CC);
                                break;
                            default:
                                assert(0);
                        }
#ifdef ZEND_ENGINE_2
                        efp->common.prototype = NULL;
                        efp->common.fn_flags = sfp->common.fn_flags & (~ZEND_ACC_IMPLEMENTED_ABSTRACT);
#endif
                        apc_swizzle_ptr(bd, &ll, &ep->val.file.functions[fcount].name);
                        apc_swizzle_ptr(bd, &ll, (void**)&ep->val.file.functions[fcount].function);
                        apc_swizzle_op_array(bd, &ll, &efp->op_array TSRMLS_CC);
                    }


                    for(ep->num_classes=0; sp->value->data.file.classes[ep->num_classes].class_entry != NULL;) { ep->num_classes++; }
                    ep->val.file.classes = apc_bd_alloc(sizeof(apc_class_t) * ep->num_classes TSRMLS_CC);
                    for(fcount=0; fcount < ep->num_classes; fcount++) {
                        ep->val.file.classes[fcount].name = apc_xmemcpy(sp->value->data.file.classes[fcount].name, sp->value->data.file.classes[fcount].name_len + 1, apc_bd_alloc TSRMLS_CC);
                        ep->val.file.classes[fcount].name_len = sp->value->data.file.classes[fcount].name_len;
                        ep->val.file.classes[fcount].class_entry = apc_copy_class_entry(NULL, sp->value->data.file.classes[fcount].class_entry, &ctxt TSRMLS_CC);
                        ep->val.file.classes[fcount].parent_name = apc_xstrdup(sp->value->data.file.classes[fcount].parent_name, apc_bd_alloc TSRMLS_CC);

                        apc_swizzle_ptr(bd, &ll, &ep->val.file.classes[fcount].name);
                        apc_swizzle_ptr(bd, &ll, &ep->val.file.classes[fcount].parent_name);
                        apc_swizzle_class_entry(bd, &ll, ep->val.file.classes[fcount].class_entry TSRMLS_CC);
                        apc_swizzle_ptr(bd, &ll, &ep->val.file.classes[fcount].class_entry);
                    }

                    apc_swizzle_ptr(bd, &ll, &bd->entries[count].val.file.filename);
                    apc_swizzle_op_array(bd, &ll, bd->entries[count].val.file.op_array TSRMLS_CC);
                    apc_swizzle_ptr(bd, &ll, &bd->entries[count].val.file.op_array);
                    apc_swizzle_ptr(bd, &ll, (void**)&ep->val.file.functions);
                    apc_swizzle_ptr(bd, &ll, (void**)&ep->val.file.classes);

                    count++;
                } else {
                    /* TODO: Currently we don't support APC_CACHE_KEY_FILE type.  We need to store the path and re-stat on load */
                }
            }
        }
    }

    /* append swizzle pointer list to bd */
    bd = apc_swizzle_bd(bd, &ll TSRMLS_CC);
    zend_llist_destroy(&ll);
    zend_hash_destroy(&APCG(apc_bd_alloc_list));

    if(files) {
        zend_hash_destroy(files);
        efree(files);
    }
    if(user_vars) {
        zend_hash_destroy(user_vars);
        efree(user_vars);
    }

    efree(pool_ptr);

    return bd;
} /* }}} */

/* {{{ apc_bin_load */
int apc_bin_load(apc_bd_t *bd, int flags TSRMLS_DC) {
    apc_bd_entry_t *ep;
    uint i, i2;
    int ret;
    time_t t;
    zend_op_array *alloc_op_array = NULL;
    apc_function_t *alloc_functions = NULL;
    apc_class_t *alloc_classes = NULL;
    apc_cache_entry_t *cache_entry;
    apc_cache_key_t cache_key;
    apc_context_t ctxt;

    if (bd->swizzled) {
        if(apc_unswizzle_bd(bd, flags TSRMLS_CC) < 0) {
            return -1;
        }
    }

    t = apc_time();

    for(i = 0; i < bd->num_entries; i++) {
        ctxt.pool = apc_pool_create(APC_SMALL_POOL, apc_sma_malloc, apc_sma_free, apc_sma_protect, apc_sma_unprotect TSRMLS_CC);
        if (!ctxt.pool) { /* TODO need to cleanup previous pools */
            apc_warning("Unable to allocate memory for pool." TSRMLS_CC);
            goto failure;
        }
        ep = &bd->entries[i];
        switch (ep->type) {
            case APC_CACHE_KEY_FILE:
                /* TODO: Currently we don't support APC_CACHE_KEY_FILE type.  We need to store the path and re-stat on load (or something else perhaps?) */
                break;
            case APC_CACHE_KEY_FPFILE:
                ctxt.copy = APC_COPY_IN_OPCODE;

                HANDLE_BLOCK_INTERRUPTIONS();
#if NONBLOCKING_LOCK_AVAILABLE
                if(APCG(write_lock)) {
                    if(!apc_cache_write_lock(apc_cache TSRMLS_CC)) {
                        HANDLE_UNBLOCK_INTERRUPTIONS();
                        return -1;
                    }
                }
#endif
                if(! (alloc_op_array = apc_copy_op_array(NULL, ep->val.file.op_array, &ctxt TSRMLS_CC))) {
                    goto failure;
                }
                apc_bin_fixup_op_array(alloc_op_array);

                if(! (alloc_functions = apc_sma_malloc(sizeof(apc_function_t) * (ep->num_functions + 1) TSRMLS_CC))) {
                    goto failure;
                }
                for(i2=0; i2 < ep->num_functions; i2++) {
                    if(! (alloc_functions[i2].name = apc_xmemcpy(ep->val.file.functions[i2].name, ep->val.file.functions[i2].name_len + 1, apc_sma_malloc TSRMLS_CC))) {
                        goto failure;
                    }
                    alloc_functions[i2].name_len = ep->val.file.functions[i2].name_len;
                    if(! (alloc_functions[i2].function = apc_sma_malloc(sizeof(zend_function) TSRMLS_CC))) {
                        goto failure;
                    }
                    switch(ep->val.file.functions[i2].function->type) {
                        case ZEND_INTERNAL_FUNCTION:
                        case ZEND_OVERLOADED_FUNCTION:
                            alloc_functions[i2].function->op_array = ep->val.file.functions[i2].function->op_array;
                            break;
                        case ZEND_USER_FUNCTION:
                        case ZEND_EVAL_CODE:
                            if (!apc_copy_op_array(&alloc_functions[i2].function->op_array, &ep->val.file.functions[i2].function->op_array, &ctxt TSRMLS_CC)) {
                                goto failure;
                            }
                            apc_bin_fixup_op_array(&alloc_functions[i2].function->op_array);
                            break;
                        default:
                            assert(0);
                    }
#ifdef ZEND_ENGINE_2
                    alloc_functions[i2].function->common.prototype=NULL;
                    alloc_functions[i2].function->common.fn_flags=ep->val.file.functions[i2].function->common.fn_flags & (~ZEND_ACC_IMPLEMENTED_ABSTRACT);
#endif
                }
                alloc_functions[i2].name = NULL;
                alloc_functions[i2].function = NULL;

                if(! (alloc_classes = apc_sma_malloc(sizeof(apc_class_t) * (ep->num_classes + 1) TSRMLS_CC))) {
                    goto failure;
                }
                for(i2=0; i2 < ep->num_classes; i2++) {
                    if(! (alloc_classes[i2].name = apc_xmemcpy(ep->val.file.classes[i2].name, ep->val.file.classes[i2].name_len+1, apc_sma_malloc TSRMLS_CC))) {
                        goto failure;
                    }
                    alloc_classes[i2].name_len = ep->val.file.classes[i2].name_len;
                    if(! (alloc_classes[i2].class_entry = apc_copy_class_entry(NULL, ep->val.file.classes[i2].class_entry, &ctxt TSRMLS_CC))) {
                        goto failure;
                    }
                    apc_bin_fixup_class_entry(alloc_classes[i2].class_entry);
                    if(! (alloc_classes[i2].parent_name = apc_xstrdup(ep->val.file.classes[i2].parent_name, apc_sma_malloc TSRMLS_CC))) {
                        if(ep->val.file.classes[i2].parent_name != NULL) {
                            goto failure;
                        }
                    }
                }
                alloc_classes[i2].name = NULL;
                alloc_classes[i2].class_entry = NULL;

                if(!(cache_entry = apc_cache_make_file_entry(ep->val.file.filename, alloc_op_array, alloc_functions, alloc_classes, &ctxt TSRMLS_CC))) {
                    goto failure;
                }

                if (!apc_cache_make_file_key(&cache_key, ep->val.file.filename, PG(include_path), t TSRMLS_CC)) {
                    goto failure;
                }
                memmove(cache_key.md5, ep->file_md5, 16);

                if ((ret = apc_cache_insert(apc_cache, cache_key, cache_entry, &ctxt, t TSRMLS_CC)) != 1) {
                    if(ret==-1) {
                        goto failure;
                    }
                }

#if NONBLOCKING_LOCK_AVAILABLE
                if(APCG(write_lock)) {
                    apc_cache_write_unlock(apc_cache TSRMLS_CC);
                }
#endif
                HANDLE_UNBLOCK_INTERRUPTIONS();

                break;
            case APC_CACHE_KEY_USER:
                {
                    zval *data;
                    uint use_copy = 0;
                    switch (Z_TYPE_P(ep->val.user.val)) {
                        case IS_OBJECT:
                            ctxt.copy = APC_COPY_OUT_USER;
                            data = apc_copy_zval(NULL, ep->val.user.val, &ctxt TSRMLS_CC);
                            use_copy = 1;
                        break;
                        default:
                            data = ep->val.user.val;
                        break;
                    }
                    ctxt.copy = APC_COPY_IN_USER;
                    _apc_store(ep->val.user.info, ep->val.user.info_len, data, ep->val.user.ttl, 0 TSRMLS_CC);
                    if (use_copy) {
                        zval_ptr_dtor(&data);
                    }
                }
                break;
            default:
                break;
       }
    }

    return 0;

failure:
    apc_pool_destroy(ctxt.pool TSRMLS_CC);
    apc_warning("Unable to allocate memory for apc binary load/dump functionality." TSRMLS_CC);
#if NONBLOCKING_LOCK_AVAILABLE
    if(APCG(write_lock)) {
        apc_cache_write_unlock(apc_cache TSRMLS_CC);
    }
#endif
    HANDLE_UNBLOCK_INTERRUPTIONS();
    return -1;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
