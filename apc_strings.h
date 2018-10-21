/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2018 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Nikita Popov <nikic@php.net>                                |
  +----------------------------------------------------------------------+
 */

#ifndef APC_STRINGS_H
#define APC_STRINGS_H

#define APC_STRINGS \
	X(access_time) \
	X(creation_time) \
	X(deletion_time) \
	X(hits) \
	X(info) \
	X(key) \
	X(mem_size) \
	X(mtime) \
	X(num_hits) \
	X(ref_count) \
	X(refs) \
	X(ttl) \
	X(type) \
	X(user) \
	X(value) \

#define X(str) extern zend_string *apc_str_ ## str;
	APC_STRINGS
#undef X

#endif
