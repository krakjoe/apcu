--TEST--
Check apc.mmap_hugetlb_mode parameter
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc'); 
if (PHP_OS != "Linux") die("skip only on Linux");
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.mmap_hugetlb_page_size=2M
apc.shm_size=32M
--FILE--
<?php
var_dump(ini_get('apc.mmap_hugetlb_page_size'));
?>
===DONE===
--EXPECT--
string(2) "2M"
===DONE===
