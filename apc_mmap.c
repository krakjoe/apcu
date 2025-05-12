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
  | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

#include "apc.h"
#include "apc_mmap.h"
#include "apc_lock.h"

#ifdef APC_MMAP

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>

/*
 * Some operating systems (like FreeBSD) have a MAP_NOSYNC flag that
 * tells whatever update daemons might be running to not flush dirty
 * vm pages to disk unless absolutely necessary.  My guess is that
 * most systems that don't have this probably default to only synching
 * to disk when absolutely necessary.
 */
#ifndef MAP_NOSYNC
#define MAP_NOSYNC 0
#endif

/* support for systems where MAP_ANONYMOUS is defined but not MAP_ANON, ie: HP-UX bug #14615 */
#if !defined(MAP_ANON) && defined(MAP_ANONYMOUS)
# define MAP_ANON MAP_ANONYMOUS
#endif

static int apc_mmap_hugepage_flags(zend_long hugepage_size)
{
	zend_long page_size = hugepage_size;
	int log2_page_size = -1;

	if (hugepage_size == -1) return 0;           // not use hugepages

#if !defined(MAP_HUGETLB) || !defined(MAP_HUGE_MASK) || !defined(MAP_HUGE_SHIFT)
	apc_warning("This system does not support hugepages");
	return 0;
#else
	if (hugepage_size == 0)  return MAP_HUGETLB; // use kernel default hugepage size

	// calculate log2 of hugepage size
	while (page_size) {
		page_size >>= 1;
		log2_page_size++;
	}

	if ((log2_page_size & MAP_HUGE_MASK) != log2_page_size) {
		// maybe hugepage size is too large
		apc_warning("Invalid hugepage size: %ld, using default hugepage size", hugepage_size);
		return MAP_HUGETLB;
	}

	return MAP_HUGETLB | ((unsigned int)log2_page_size << MAP_HUGE_SHIFT);
#endif
}

void *apc_mmap(char *file_mask, size_t size, zend_long hugepage_size)
{
	void *shmaddr;
	int fd = -1;
	int flags = MAP_SHARED | MAP_NOSYNC;

	/* If no filename was provided, do an anonymous mmap */
	if (!file_mask || (file_mask && !strlen(file_mask))) {
#if !defined(MAP_ANON)
		zend_error_noreturn(E_CORE_ERROR, "Anonymous mmap does not appear to be available on this system (MAP_ANON/MAP_ANONYMOUS).  Please see the apc.mmap_file_mask INI option.");
#else
		fd = -1;
		flags = MAP_SHARED | MAP_ANON;
#endif
	} else if (!strcmp(file_mask,"/dev/zero")) {
		fd = open("/dev/zero", O_RDWR, S_IRUSR | S_IWUSR);
		if (fd == -1) {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: open on /dev/zero failed");
		}
	} else {
		/* Otherwise we do a normal filesystem mmap */
		fd = mkstemp(file_mask);
		if (fd == -1) {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: mkstemp on %s failed", file_mask);
		}
		if (ftruncate(fd, size) < 0) {
			close(fd);
			unlink(file_mask);
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: ftruncate failed");
		}
		unlink(file_mask);
	}

	flags |= apc_mmap_hugepage_flags(hugepage_size);
	shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, flags, fd, 0);

	if ((long)shmaddr == -1) {
		if (hugepage_size >= 0) {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: Failed to mmap %zu bytes with hugepage size %ld. apc.shm_size may be too large, apc.mmap_hugepage_size may be invalid, or the system lacks sufficient reserved hugepages.", size, hugepage_size);
		} else {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: Failed to mmap %zu bytes. apc.shm_size may be too large.", size);
		}
	}

#ifdef MADV_HUGEPAGE
	/* enable transparent huge pages to reduce TLB misses (Linux only) */
	madvise(shmaddr, size, MADV_HUGEPAGE);
#endif

	if (fd != -1) close(fd);

	return shmaddr;
}

void apc_unmap(void *shmaddr, size_t size)
{
	if (munmap(shmaddr, size) < 0) {
		apc_warning("apc_unmap: munmap failed");
	}
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: noexpandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4 sts=4
 */
