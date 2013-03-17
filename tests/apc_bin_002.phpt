--TEST--
APC: bindump user cache
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
<?php
/* this is a hack, but otherwise the data for each platform have to be created. */
apc_clear_cache();
apc_store('foo', 42);
apc_bin_dumpfile(array('foo'), dirname(__FILE__) . '/foo.bin');
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apc_clear_cache();
var_dump(apc_fetch('foo'));
apc_bin_loadfile(dirname(__FILE__) . '/foo.bin');
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
===DONE===
