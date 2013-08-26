--TEST--
APC: bindump user cache, variation 4 (check apc_bin_* aliases work)
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apc_clear_cache();
apc_store('foo', 42);
apc_bin_dumpfile(array('this', 'is', 'ignored'), array('foo'), dirname(__FILE__) . '/foo.bin', APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
apc_clear_cache();
var_dump(apc_fetch('foo'));
apc_bin_loadfile(dirname(__FILE__) . '/foo.bin', NULL, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
var_dump(apc_fetch('foo'));

apc_clear_cache();
apc_store('foo', 'bar');
$dump = apc_bin_dump(array('this', 'is', 'ignored'), array('foo'));
apc_clear_cache();
var_dump(apc_fetch('foo'));
apc_bin_load($dump, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
var_dump(apc_fetch('foo'));
?>
===DONE===
<?php exit(0); ?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . '/foo.bin');
--EXPECTF--
bool(false)
int(42)
bool(false)
string(3) "bar"
===DONE===
