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
  | Authors: Daniel Cowgill <dcowgill@communityconnect.com>              |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_zend.h 326712 2012-07-19 21:33:27Z rasmus $ */

#ifndef APC_ZEND_H
#define APC_ZEND_H

/* Utilities for interfacing with the zend engine */

#include "apc.h"
#include "apc_php.h"

#ifndef Z_REFCOUNT_P
#define Z_REFCOUNT_P(pz)              (pz)->refcount
#define Z_REFCOUNT_PP(ppz)            Z_REFCOUNT_P(*(ppz))
#endif

#ifndef Z_SET_REFCOUNT_P
#define Z_SET_REFCOUNT_P(pz, rc)      (pz)->refcount = rc
#define Z_SET_REFCOUNT_PP(ppz, rc)    Z_SET_REFCOUNT_P(*(ppz), rc)
#endif

#ifndef Z_ADDREF_P
#define Z_ADDREF_P(pz)                (pz)->refcount++
#define Z_ADDREF_PP(ppz)              Z_ADDREF_P(*(ppz))
#endif

#ifndef Z_DELREF_P
#define Z_DELREF_P(pz)                (pz)->refcount--
#define Z_DELREF_PP(ppz)              Z_DELREF_P(*(ppz))
#endif

#ifndef Z_ISREF_P
#define Z_ISREF_P(pz)                 (pz)->is_ref
#define Z_ISREF_PP(ppz)               Z_ISREF_P(*(ppz))
#endif

#ifndef Z_SET_ISREF_P
#define Z_SET_ISREF_P(pz)             (pz)->is_ref = 1
#define Z_SET_ISREF_PP(ppz)           Z_SET_ISREF_P(*(ppz))
#endif

#ifndef Z_UNSET_ISREF_P
#define Z_UNSET_ISREF_P(pz)           (pz)->is_ref = 0
#define Z_UNSET_ISREF_PP(ppz)         Z_UNSET_ISREF_P(*(ppz))
#endif

#ifndef Z_SET_ISREF_TO_P
#define Z_SET_ISREF_TO_P(pz, isref)   (pz)->is_ref = isref
#define Z_SET_ISREF_TO_PP(ppz, isref) Z_SET_ISREF_TO_P(*(ppz), isref)
#endif


extern void* apc_php_malloc(size_t n TSRMLS_DC);
extern void apc_php_free(void* p TSRMLS_DC);

extern void apc_zend_init(TSRMLS_D);
extern void apc_zend_shutdown(TSRMLS_D);


/* offset for apc info in op_array->reserved */
extern int apc_reserved_offset;

#ifndef ZEND_VM_KIND_CALL /* Not currently defined by any ZE version */
# define ZEND_VM_KIND_CALL  1
#endif

#ifndef ZEND_VM_KIND /* Indicates PHP < 5.1 */
# define ZEND_VM_KIND   ZEND_VM_KIND_CALL
#endif

#if defined(ZEND_ENGINE_2) && (ZEND_VM_KIND == ZEND_VM_KIND_CALL)
# define APC_OPCODE_OVERRIDE
#endif

#ifdef APC_OPCODE_OVERRIDE

#ifdef ZEND_ENGINE_2_1
/* Taken from Zend/zend_vm_execute.h */
#define _CONST_CODE  0
#define _TMP_CODE    1
#define _VAR_CODE    2
#define _UNUSED_CODE 3
#define _CV_CODE     4
static inline int _apc_opcode_handler_decode(zend_op *opline)
{
    static const int apc_vm_decode[] = {
        _UNUSED_CODE, /* 0              */
        _CONST_CODE,  /* 1 = IS_CONST   */
        _TMP_CODE,    /* 2 = IS_TMP_VAR */
        _UNUSED_CODE, /* 3              */
        _VAR_CODE,    /* 4 = IS_VAR     */
        _UNUSED_CODE, /* 5              */
        _UNUSED_CODE, /* 6              */
        _UNUSED_CODE, /* 7              */
        _UNUSED_CODE, /* 8 = IS_UNUSED  */
        _UNUSED_CODE, /* 9              */
        _UNUSED_CODE, /* 10             */
        _UNUSED_CODE, /* 11             */
        _UNUSED_CODE, /* 12             */
        _UNUSED_CODE, /* 13             */
        _UNUSED_CODE, /* 14             */
        _UNUSED_CODE, /* 15             */
        _CV_CODE      /* 16 = IS_CV     */
    };
#ifdef ZEND_ENGINE_2_4
    return (opline->opcode * 25) + (apc_vm_decode[opline->op1_type] * 5) + apc_vm_decode[opline->op2_type];
#else
    return (opline->opcode * 25) + (apc_vm_decode[opline->op1.op_type] * 5) + apc_vm_decode[opline->op2.op_type];
#endif
}

# define APC_ZEND_OPLINE                    zend_op *opline = execute_data->opline;
# define APC_OPCODE_HANDLER_DECODE(opline)  _apc_opcode_handler_decode(opline)
# if PHP_MAJOR_VERSION >= 6
#  define APC_OPCODE_HANDLER_COUNT          ((25 * 152) + 1)
# elif defined(ZEND_ENGINE_2_4)
#  define APC_OPCODE_HANDLER_COUNT          ((25 * 159) + 1) /* 5 new opcodes in 5.4 - qm_assign_var, jmp_set_var, separate, bind_trais, add_trait */
# elif PHP_MAJOR_VERSION >= 5 && PHP_MINOR_VERSION >= 3
#  define APC_OPCODE_HANDLER_COUNT          ((25 * 154) + 1) /* 3 new opcodes in 5.3 - unused, lambda, jmp_set */
# else
#  define APC_OPCODE_HANDLER_COUNT          ((25 * 151) + 1)
# endif
# define APC_REPLACE_OPCODE(opname)         { int i; for(i = 0; i < 25; i++) if (zend_opcode_handlers[(opname*25) + i]) zend_opcode_handlers[(opname*25) + i] = apc_op_##opname; }

#else /* ZE2.0 */
# define APC_ZEND_ONLINE
# define APC_OPCODE_HANDLER_DECODE(opline)  (opline->opcode)
# define APC_OPCODE_HANDLER_COUNT           512
# define APC_REPLACE_OPCODE(opname)         zend_opcode_handlers[opname] = apc_op_##opname;
#endif

#ifndef ZEND_FASTCALL  /* Added in ZE2.3.0 */
#define ZEND_FASTCALL
#endif

/* Added in ZE2.3.0 */
#ifndef zend_parse_parameters_none
# define zend_parse_parameters_none() zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "")
#endif


#endif  /* APC_OPCODE_OVERRIDE */

#ifdef ZEND_ENGINE_2_4
# define ZEND_CE_FILENAME(ce)			(ce)->info.user.filename
# define ZEND_CE_DOC_COMMENT(ce)        (ce)->info.user.doc_comment
# define ZEND_CE_DOC_COMMENT_LEN(ce)	(ce)->info.user.doc_comment_len
# define ZEND_CE_BUILTIN_FUNCTIONS(ce)  (ce)->info.internal.builtin_functions
#else
# define ZEND_CE_FILENAME(ce)			(ce)->filename
# define ZEND_CE_DOC_COMMENT(ce)        (ce)->doc_comment
# define ZEND_CE_DOC_COMMENT_LEN(ce)	(ce)->doc_comment_len
# define ZEND_CE_BUILTIN_FUNCTIONS(ce)  (ce)->builtin_functions
#endif

#endif  /* APC_ZEND_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
