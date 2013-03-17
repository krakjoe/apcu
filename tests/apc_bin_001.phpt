--TEST--
APC: bindump user cache, variation 1
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apc_clear_cache();
apc_store('foo', 42);
$dump = apcu_bin_dump(array('foo'));
apc_clear_cache();
var_dump(apc_fetch('foo'));
apcu_bin_load($dump, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
var_dump(apc_fetch('foo'));
?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
bool(false)
int(42)
===DONE===
