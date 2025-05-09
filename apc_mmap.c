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

#if defined(__linux__)
# include <linux/mman.h>
#endif

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

#if defined(__linux__)
static int apc_mmap_hugetlb_flags(char *mmap_hugetlb_mode)
{
	int flags;

	if (!mmap_hugetlb_mode
		|| (mmap_hugetlb_mode && !strlen(mmap_hugetlb_mode))
		|| !strcasecmp(mmap_hugetlb_mode, "none")) {
		return 0;
	}

# ifndef MAP_HUGETLB
	flags = 0;
	apc_warning("MAP_HUGETLB is not defined on this system");
# else
	flags = MAP_HUGETLB;
	if (!strcasecmp(mmap_hugetlb_mode, "default")) {
		return flags;
	}
#  ifdef MAP_HUGE_16KB
	if (!strcasecmp(mmap_hugetlb_mode, "16kb")) {
		flags |= MAP_HUGE_16KB;
	} else
#  endif
#  ifdef MAP_HUGE_64KB
	if (!strcasecmp(mmap_hugetlb_mode, "64kb")) {
		flags |= MAP_HUGE_64KB;
	} else
#  endif
#  ifdef MAP_HUGE_512KB
	if (!strcasecmp(mmap_hugetlb_mode, "512kb")) {
		flags |= MAP_HUGE_512KB;
	} else
#  endif
#  ifdef MAP_HUGE_1MB
	if (!strcasecmp(mmap_hugetlb_mode, "1mb")) {
		flags |= MAP_HUGE_1MB;
	} else
#  endif
#  ifdef MAP_HUGE_2MB
	if (!strcasecmp(mmap_hugetlb_mode, "2mb")) {
		flags |= MAP_HUGE_2MB;
	} else
#  endif
#  ifdef MAP_HUGE_8MB
	if (!strcasecmp(mmap_hugetlb_mode, "8mb")) {
		flags |= MAP_HUGE_8MB;
	} else
#  endif
#  ifdef MAP_HUGE_16MB
	if (!strcasecmp(mmap_hugetlb_mode, "16mb")) {
		flags |= MAP_HUGE_16MB;
	} else
#  endif
#  ifdef MAP_HUGE_32MB
	if (!strcasecmp(mmap_hugetlb_mode, "32mb")) {
		flags |= MAP_HUGE_32MB;
	} else
#  endif
#  ifdef MAP_HUGE_256MB
	if (!strcasecmp(mmap_hugetlb_mode, "256mb")) {
		flags |= MAP_HUGE_256MB;
	} else
#  endif
#  ifdef MAP_HUGE_512MB
	if (!strcasecmp(mmap_hugetlb_mode, "512mb")) {
		flags |= MAP_HUGE_512MB;
	} else
#  endif
#  ifdef MAP_HUGE_1GB
	if (!strcasecmp(mmap_hugetlb_mode, "1gb")) {
		flags |= MAP_HUGE_1GB;
	} else
#  endif
#  ifdef MAP_HUGE_2GB
	if (!strcasecmp(mmap_hugetlb_mode, "2gb")) {
		flags |= MAP_HUGE_2GB;
	} else
#  endif
#  ifdef MAP_HUGE_16GB
	if (!strcasecmp(mmap_hugetlb_mode, "16gb")) {
		flags |= MAP_HUGE_16GB;
	} else
#  endif
	{
		apc_warning("Invalid huge page size: %s, using default huge page size.", mmap_hugetlb_mode);
	}
# endif
	return flags;
}
#endif

void *apc_mmap(char *file_mask, size_t size, char *mmap_hugetlb_mode)
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

#if defined(__linux__)
	flags |= apc_mmap_hugetlb_flags(mmap_hugetlb_mode);
#endif

	shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, flags, fd, 0);

	if ((long)shmaddr == -1) {
#if defined(__linux__)
		if (mmap_hugetlb_mode && strlen(mmap_hugetlb_mode)) {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: Failed to mmap %zu bytes with huge page size %s. apc.shm_size may be too large, apc.mmap_hugetlb_mode may be invalid, or the system lacks sufficient reserved HugePages.", size, mmap_hugetlb_mode);
		} else
#endif
		{
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
