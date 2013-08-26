--TEST--
APC: bindump user cache, variation 3
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
<?php
/* XXX this is a hack, but otherwise the data for each platform 
   has te be created manually */
apc_clear_cache();
apc_store('foo', 42);
apc_store('bar', 'foo');
apcu_bin_dumpfile(NULL, dirname(__FILE__) . '/foo.bin', APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apc_clear_cache();
var_dump(apc_fetch('foo'));
var_dump(apc_fetch('bar'));
apcu_bin_loadfile(dirname(__FILE__) . '/foo.bin', NULL, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
var_dump(apc_fetch('foo'));
var_dump(apc_fetch('bar'));
?>
===DONE===
<?php exit(0); ?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . '/foo.bin');
--EXPECTF--
bool(false)
bool(false)
int(42)
string(3) "foo"
===DONE===
