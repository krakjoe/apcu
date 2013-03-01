--TEST--
Bug #63434 (Segfault if apc.shm_strings_buffer excceed apc.shm_size)
--SKIPIF--
<?php
if (!extension_loaded("apc")) die("skip");
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=1M
apc.shm_strings_buffer=2M
--FILE--
<?php
$tmp = dirname(__FILE__) . "/bug3434.tmp";
file_put_contents($tmp, "<?php return array('xxx' => 'xxxx'); ?>");
include $tmp;

echo "okey";
?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . "/bug3434.tmp");
?>
--EXPECT--
PHP Warning:  PHP Startup: apc.shm_strings_buffer '2097152' exceed apc.shm_size '1048576' in Unknown on line 0

Warning: PHP Startup: apc.shm_strings_buffer '2097152' exceed apc.shm_size '1048576' in Unknown on line 0
okey
