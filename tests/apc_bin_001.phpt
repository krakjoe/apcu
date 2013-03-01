--TEST--
APC: bindump user cache
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apc_clear_cache('file');
apc_store('testkey','testvalue');
apc_bin_dump();
$dump = apc_bin_dump(array(), NULL);
apc_clear_cache('user');
var_dump(apc_fetch('testkey'));
apc_bin_load($dump, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
var_dump(apc_fetch('testkey'));
?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
bool(false)
string(9) "testvalue"
===DONE===
