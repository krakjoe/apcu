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
  | Authors: Gopal Vijayaraghavan <gopalv@yahoo-inc.com>                 |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Yahoo! Inc. in 2008.

   Future revisions and derivatives of this source code must acknowledge
   Yahoo! Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

#include "apc_pool.h"
#include <assert.h>

#ifdef APC_POOL_DEBUG
# ifdef HAVE_VALGRIND_MEMCHECK_H
#  include <valgrind/memcheck.h>
# endif
#endif

/*
   parts in ? are optional and turned on for fun, memory loss,
   and for something else that I forgot about ... ah, debugging

				 |--------> data[]         |<-- non word boundary (too)
   +-------------+--------------+-----------+-------------+-------------->>>
   | pool_block  | ?sizeinfo<1> | block<1>  | ?redzone<1> | ?sizeinfo<2>
   |             |  (size_t)    |           | padded left |
   +-------------+--------------+-----------+-------------+-------------->>>
 */
typedef struct _pool_block
{
	size_t              avail;
	size_t              capacity;
	unsigned char       *mark;
	struct _pool_block  *next;
	unsigned             :0; /* this should align to word */
	/* data comes here */
} pool_block;

struct _apc_pool {
	/* denotes the size and debug flags for a pool */
	apc_pool_type   type;

	/* handler functions */
	apc_malloc_t    allocate;
	apc_free_t      deallocate;

	apc_protect_t   protect;
	apc_unprotect_t unprotect;

	/* total */
	size_t          size;
	/* remaining */
	size_t          used;

	size_t     dsize;

	unsigned long count;

	pool_block *head;
	pool_block first;
};

/* {{{ redzone code */
static const unsigned char decaff[] =  {
	0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
	0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
	0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad,
	0xde, 0xca, 0xff, 0xc0, 0xff, 0xee, 0xba, 0xad
};

/* a redzone is at least 4 (0xde,0xca,0xc0,0xff) bytes */
#define REDZONE_SIZE(size) \
	((ALIGNWORD((size)) > ((size) + 4)) ? \
		(ALIGNWORD((size)) - (size)) : /* does not change realsize */\
		ALIGNWORD((size)) - (size) + ALIGNWORD((sizeof(char)))) /* adds 1 word to realsize */

#define SIZEINFO_SIZE ALIGNWORD(sizeof(size_t))

#define MARK_REDZONE(block, redsize) do {\
	   memcpy(block, decaff, redsize );\
	} while(0)

#define CHECK_REDZONE(block, redsize) (memcmp(block, decaff, redsize) == 0)

/* }}} */

#define INIT_POOL_BLOCK(rpool, entry, size) do {\
	(entry)->avail = (entry)->capacity = (size);\
	(entry)->mark =  ((unsigned char*)(entry)) + ALIGNWORD(sizeof(pool_block));\
	(entry)->next = (rpool)->head;\
	(rpool)->head = (entry);\
} while(0)

/* {{{ create_pool_block */
static pool_block* create_pool_block(apc_pool *pool,
									 size_t size)
{
	apc_malloc_t allocate = pool->allocate;

	size_t realsize = sizeof(pool_block) + ALIGNWORD(size);

	pool_block* entry = allocate(realsize);

	if (!entry) {
		return NULL;
	}

	INIT_POOL_BLOCK(pool, entry, size);

	pool->size += realsize;
	pool->count++;

	return entry;
}
/* }}} */

/* {{{ apc_pool_alloc */
PHP_APCU_API void* apc_pool_alloc(apc_pool *pool, size_t size)
{
	unsigned char *p = NULL;
	size_t realsize = ALIGNWORD(size);
	size_t poolsize;
	unsigned char *redzone  = NULL;
	size_t redsize  = 0;
	size_t *sizeinfo= NULL;
	pool_block *entry = NULL;
	unsigned long i;

	if(APC_POOL_HAS_REDZONES(pool)) {
		redsize = REDZONE_SIZE(size); /* redsize might be re-using word size padding */
		realsize = size + redsize;    /* recalculating realsize */
	} else {
		redsize = realsize - size; /* use padding space */
	}

	if(APC_POOL_HAS_SIZEINFO(pool)) {
		realsize += ALIGNWORD(sizeof(size_t));
	}

	/* minimize look-back, a value of 8 seems to give similar fill-ratios (+2%)
	 * as looping through the entire list. And much faster in allocations. */
	for(entry = pool->head, i = 0; entry != NULL && (i < 8); entry = entry->next, i++) {
		if(entry->avail >= realsize) {
			goto found;
		}
	}

	/* upgrade the pool type to reduce overhead */
	if(pool->count > 4 && pool->dsize < 4096) {
		pool->dsize = 4096;
	} else if(pool->count > 8 && pool->dsize < 8192) {
		pool->dsize = 8192;
	}

	poolsize = ALIGNSIZE(realsize, pool->dsize);

	entry = create_pool_block(pool, poolsize);

	if(!entry) {
		return NULL;
	}

found:
	p = entry->mark;

	if(APC_POOL_HAS_SIZEINFO(pool)) {
		sizeinfo = (size_t*)p;
		p += SIZEINFO_SIZE;
		*sizeinfo = size;
	}

	redzone = p + size;

	if(APC_POOL_HAS_REDZONES(pool)) {
		MARK_REDZONE(redzone, redsize);
	}

#ifdef VALGRIND_MAKE_MEM_NOACCESS
	if(redsize != 0) {
		VALGRIND_MAKE_MEM_NOACCESS(redzone, redsize);
	}
#endif

	entry->avail -= realsize;
	entry->mark  += realsize;
	pool->used   += realsize;

#ifdef VALGRIND_MAKE_MEM_UNDEFINED
	/* need to write before reading data off this */
	VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif

	return (void*)p;
}
/* }}} */

/* {{{ apc_pool_check_integrity */
/*
 * Checking integrity at runtime, does an
 * overwrite check only when the sizeinfo
 * is set.
 *
 * Marked as used in gcc, so that this function
 * is accessible from gdb, eventhough it is never
 * used in code in non-debug builds.
 */
static APC_USED int apc_pool_check_integrity(apc_pool *pool)
{
	pool_block *entry;
	size_t *sizeinfo = NULL;
	unsigned char *start;
	size_t realsize;
	unsigned char   *redzone;
	size_t redsize;

	for(entry = pool->head; entry != NULL; entry = entry->next) {
		start = (unsigned char *)entry + ALIGNWORD(sizeof(pool_block));
		if((entry->mark - start) != (entry->capacity - entry->avail)) {
			return 0;
		}
	}

	if(!APC_POOL_HAS_REDZONES(pool) ||
		!APC_POOL_HAS_SIZEINFO(pool)) {
		(void)pool; /* remove unused warning */
		return 1;
	}

	for(entry = pool->head; entry != NULL; entry = entry->next) {
		start = (unsigned char *)entry + ALIGNWORD(sizeof(pool_block));

		while(start < entry->mark) {
			sizeinfo = (size_t*)start;
			/* redzone starts where real data ends, in a non-word boundary
			 * redsize is at least 4 bytes + whatever's needed to make it
			 * to another word boundary.
			 */
			redzone = start + SIZEINFO_SIZE + (*sizeinfo);
			redsize = REDZONE_SIZE(*sizeinfo);
#ifdef VALGRIND_MAKE_MEM_DEFINED
			VALGRIND_MAKE_MEM_DEFINED(redzone, redsize);
#endif
			if(!CHECK_REDZONE(redzone, redsize))
			{
				/*
				fprintf(stderr, "Redzone check failed for %p\n",
								start + ALIGNWORD(sizeof(size_t)));*/
				return 0;
			}
#ifdef VALGRIND_MAKE_MEM_NOACCESS
			VALGRIND_MAKE_MEM_NOACCESS(redzone, redsize);
#endif
			realsize = SIZEINFO_SIZE + *sizeinfo + redsize;
			start += realsize;
		}
	}

	return 1;
}
/* }}} */

/* {{{ apc_pool_free */
/*
 * free does not do anything
 */
PHP_APCU_API void apc_pool_free(apc_pool *pool, void *p)
{
}
/* }}} */

/* {{{ apc_pool_cleanup */
static void apc_pool_cleanup(apc_pool *pool)
{
	pool_block *entry;
	pool_block *tmp;
	apc_free_t deallocate = pool->deallocate;

	assert(apc_pool_check_integrity(pool)!=0);

	entry = pool->head;
	while (entry->next != NULL) {
		tmp = entry->next;
		deallocate(entry);
		entry = tmp;
	}
}
/* }}} */

/* {{{ apc_pool_destroy */
PHP_APCU_API void apc_pool_destroy(apc_pool *pool)
{
	apc_free_t deallocate = pool->deallocate;

	apc_pool_cleanup(pool);
	deallocate(pool);
}
/* }}} */


/* {{{ apc_pool_create */
PHP_APCU_API apc_pool* apc_pool_create(
        apc_pool_type type, apc_malloc_t allocate, apc_free_t deallocate,
        apc_protect_t protect, apc_unprotect_t unprotect)
{
	size_t dsize = 0;
	apc_pool *pool;

	switch (type & APC_POOL_SIZE_MASK) {
		case APC_SMALL_POOL:
			dsize = 512;
			break;

		case APC_LARGE_POOL:
			dsize = 8192;
			break;

		case APC_MEDIUM_POOL:
			dsize = 4096;
			break;

		default:
			return NULL;
	}

	pool = allocate((sizeof(apc_pool) + ALIGNWORD(dsize)));

	if (!pool) {
		return NULL;
	}

	pool->type = type;

	pool->allocate = allocate;
	pool->deallocate = deallocate;

	pool->size = sizeof(apc_pool) + ALIGNWORD(dsize);

	pool->protect = protect;
	pool->unprotect = unprotect;

	pool->dsize = dsize;
	pool->head = NULL;
	pool->count = 0;

	INIT_POOL_BLOCK(pool, &(pool->first), dsize);

	return pool;
}
/* }}} */

PHP_APCU_API size_t apc_pool_size(apc_pool *pool) {
	return pool->size;
}

/* }}} */

/* {{{ apc_pool_init */
PHP_APCU_API void apc_pool_init()
{
	/* put all ye sanity checks here */
	assert(sizeof(decaff) > REDZONE_SIZE(ALIGNWORD(sizeof(char))));
	assert(sizeof(pool_block) == ALIGNWORD(sizeof(pool_block)));
#if APC_POOL_DEBUG
	assert((APC_POOL_SIZE_MASK & (APC_POOL_SIZEINFO | APC_POOL_REDZONES)) == 0);
#endif
}
/* }}} */

/* {{{ apc_pmemcpy */
PHP_APCU_API void* APC_ALLOC apc_pmemcpy(const void* p, size_t n, apc_pool* pool)
{
	void* q;

	if (p != NULL && (q = apc_pool_alloc(pool, n)) != NULL) {
		memcpy(q, p, n);
		return q;
	}
	return NULL;
}
/* }}} */

/* {{{ apc_pstrcpy */
PHP_APCU_API zend_string* apc_pstrcpy(zend_string *str, apc_pool* pool) {
	return apc_pstrnew(ZSTR_VAL(str), ZSTR_LEN(str), pool);
} /* }}} */

/* {{{ apc_pstrnew */
PHP_APCU_API zend_string* apc_pstrnew(char *buf, size_t buf_len, apc_pool* pool) {
	zend_string* p = (zend_string *) apc_pool_alloc(pool,
		ZEND_MM_ALIGNED_SIZE(_ZSTR_STRUCT_SIZE(buf_len)));

	if (!p) {
		return NULL;
	}

	memset(p, 0, sizeof(zend_string));

#if PHP_VERSION_ID >= 70300
	GC_SET_REFCOUNT(p, 1);
	GC_TYPE_INFO(p) = IS_STRING | (IS_STR_PERSISTENT << GC_FLAGS_SHIFT);
#else
	GC_REFCOUNT(p) = 1;
	GC_TYPE_INFO(p) = IS_STRING;
	GC_FLAGS(p) = IS_STR_PERSISTENT;
#endif

	memcpy(ZSTR_VAL(p), buf, buf_len);
	ZSTR_LEN(p) = buf_len;
	ZSTR_VAL(p)[buf_len] = '\0';
	zend_string_forget_hash_val(p);

	return p;
} /* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
