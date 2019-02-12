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

#if APC_MMAP

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

apc_segment_t apc_mmap(char *file_mask, size_t size)
{
	apc_segment_t segment;

	int fd = -1;
	int flags = MAP_SHARED | MAP_NOSYNC;
#ifdef APC_MEMPROTECT
	int remap = 1;
#endif

	/* If no filename was provided, do an anonymous mmap */
	if(!file_mask || (file_mask && !strlen(file_mask))) {
#if !defined(MAP_ANON)
		zend_error_noreturn(E_CORE_ERROR, "Anonymous mmap does not appear to be available on this system (MAP_ANON/MAP_ANONYMOUS).  Please see the apc.mmap_file_mask INI option.");
#else
		fd = -1;
		flags = MAP_SHARED | MAP_ANON;
#ifdef APC_MEMPROTECT
		remap = 0;
#endif
#endif
	} else if(!strcmp(file_mask,"/dev/zero")) {
		fd = open("/dev/zero", O_RDWR, S_IRUSR | S_IWUSR);
		if(fd == -1) {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: open on /dev/zero failed");
		}
#ifdef APC_MEMPROTECT
		remap = 0; /* cannot remap */
#endif
	} else {
		/*
		 * Otherwise we do a normal filesystem mmap
		 */
		fd = mkstemp(file_mask);
		if(fd == -1) {
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: mkstemp on %s failed", file_mask);
		}
		if (ftruncate(fd, size) < 0) {
			close(fd);
			unlink(file_mask);
			zend_error_noreturn(E_CORE_ERROR, "apc_mmap: ftruncate failed");
		}
		unlink(file_mask);
	}

	segment.shmaddr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE, flags, fd, 0);
	segment.size = size;

#ifdef APC_MEMPROTECT
	if(remap) {
		segment.roaddr = (void *)mmap(NULL, size, PROT_READ, flags, fd, 0);
	} else {
		segment.roaddr = NULL;
	}
#endif

	if ((long)segment.shmaddr == -1) {
		zend_error_noreturn(E_CORE_ERROR, "apc_mmap: Failed to mmap %zu bytes. Is your apc.shm_size too large?", size);
	}

	if (fd != -1) close(fd);

	return segment;
}

void apc_unmap(apc_segment_t *segment)
{
	if (munmap(segment->shmaddr, segment->size) < 0) {
		apc_warning("apc_unmap: munmap failed");
	}

#ifdef APC_MEMPROTECT
	if (segment->roaddr && munmap(segment->roaddr, segment->size) < 0) {
		apc_warning("apc_unmap: munmap failed");
	}
#endif

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
