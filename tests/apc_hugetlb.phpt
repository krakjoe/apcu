--TEST--
Check apc.mmap_hugepage_size parameter
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc'); 

// currently only support Linux
if (PHP_OS != "Linux") die("skip only on Linux");

// check reserved hugepage
$hp = @file_get_contents('/proc/sys/vm/nr_hugepages');
if ($hp === false || !((int)trim($hp))) die("skip hugepages are currently unavailable on this system");
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.mmap_hugepage_size=2M
apc.shm_size=32M
--FILE--
<?php
if (ini_get('apc.mmap_file_mask') === false) {
    // mmap is not used in this environment
    $result = true;
} else {
    $result = ini_get('apc.mmap_hugepage_size') === '2M';
}
var_dump($result);
?>
===DONE===
--EXPECT--
bool(true)
===DONE===
