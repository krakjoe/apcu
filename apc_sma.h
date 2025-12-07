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

#ifndef APC_SMA_H
#define APC_SMA_H

/*
 * SMA API
 * APC SMA API provides support for shared memory allocators to external libraries ( and to APC )
 * Skip to the bottom macros for error free usage of the SMA API
*/

#include "apc.h"

typedef struct apc_sma_link_t apc_sma_link_t;
struct apc_sma_link_t {
	zend_long size;         /* size of this free block */
	zend_long offset;       /* offset in segment of this block */
	apc_sma_link_t* next;   /* link to next free block */
};

typedef struct apc_sma_info_t apc_sma_info_t;
struct apc_sma_info_t {
	size_t seg_size;       /* segment size */
	apc_sma_link_t* list;  /* list of free blocks */
};

typedef zend_bool (*apc_sma_expunge_f)(void *pointer, size_t size);

typedef struct _apc_sma_t {
	zend_bool initialized;         /* flag to indicate this sma has been initialized */

	/* callback */
	apc_sma_expunge_f expunge;     /* expunge */
	void** data;                   /* expunge data */

	/* info */
	size_t size;                   /* segment size */
	size_t max_alloc_size;         /* max size of memory available for allocation */
	void  *shmaddr;                /* address of shm segment */
} apc_sma_t;

/*
* apc_sma_init will initialize a shared memory allocator with the given size of shared memory
*
* should be called once per allocator per process
*/
PHP_APCU_API void apc_sma_init(
		apc_sma_t* sma, void** data, apc_sma_expunge_f expunge,
		size_t size, size_t min_alloc_size, char *mask, zend_long hugepage_size);

/*
 * apc_sma_detach will detach from shared memory and cleanup local allocations.
 */
PHP_APCU_API void apc_sma_detach(apc_sma_t* sma);

/*
* apc_smap_api_malloc will allocate a block from the sma of the given size.
* The init_callack() can be used to perform initializations that must be completed
* before the lock of the sma layer is released.
*/
typedef void (*apc_sma_malloc_init_f)(void *pointer);
PHP_APCU_API void* apc_sma_malloc(apc_sma_t* sma, size_t size, apc_sma_malloc_init_f init_callback);

/*
* apc_sma_api_free will free p (which should be a pointer to a block allocated from sma)
*/
PHP_APCU_API void apc_sma_free(apc_sma_t* sma, void* p);

/*
* apc_sma_api_info returns information about the allocator
*/
PHP_APCU_API apc_sma_info_t* apc_sma_info(apc_sma_t* sma, zend_bool limited);

/*
* apc_sma_api_info_free_info is for freeing apc_sma_info_t* returned by apc_sma_api_info
*/
PHP_APCU_API void apc_sma_free_info(apc_sma_t* sma, apc_sma_info_t* info);

/*
* apc_sma_api_get_avail_mem will return the amount of memory available left to sma
*/
PHP_APCU_API size_t apc_sma_get_avail_mem(apc_sma_t* sma);

/*
* apc_sma_check_avail returns true if at least size bytes are available across all free blocks
*/
PHP_APCU_API zend_bool apc_sma_check_avail(apc_sma_t *sma, size_t size);

/*
* apc_sma_check_avail_contiguous returns true if at least size contiguous bytes can be allocated from the sma
*/
PHP_APCU_API zend_bool apc_sma_check_avail_contiguous(apc_sma_t *sma, size_t size);

/*
* apc_sma_defrag defragments the shared memory by shifting all allocated blocks to the left,
* allowing all free blocks to be coalesced to one larger free block on the right side.
*
* The move() callback is called for each allocated block before it is moved. Therefore, move() can be used
* to prepare for the move or to prevent the block from being moved by returning 0. The argument "data" is
* passed as the first argument from apc_sma_defrag() to move(), while the old and the new address of the
* allocation is passed as the 2nd and 3rd argument. The callback must not write to the new memory area
* because the area is not yet allocated during the callback.
*/
typedef zend_bool (*apc_sma_move_f)(void *data, void *pointer_old, void *pointer_new);
PHP_APCU_API void apc_sma_defrag(apc_sma_t *sma, void *data, apc_sma_move_f move);

/* ALIGNWORD: pad up x, aligned to the system's word boundary */
#define ALIGNWORD(x) ZEND_MM_ALIGNED_SIZE(x)

#endif
