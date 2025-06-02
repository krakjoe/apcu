--TEST--
Error if apc.mmap_hugepage_size is negative
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc'); 

// is mmap used?
if (ini_get('apc.mmap_hugepage_size') === false) die("skip mmap is not used");

// currently only support Linux
if (PHP_OS != "Linux") die("skip only on Linux");

// check reserved hugepage
$hp = @file_get_contents('/proc/sys/vm/nr_hugepages');
if ($hp === false || !((int)trim($hp))) die("skip hugepages are currently unavailable on this system");
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.mmap_hugepage_size=-1
apc.shm_size=2M
--FILE--
Irrelevant
--EXPECTF--
%A: apc.mmap_hugepage_size must be a positive integer in Unknown on line 0
