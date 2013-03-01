--TEST--
APC: apc_fetch resets array pointers
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php
$items = array('bar', 'baz');

apc_store('test', $items);

$back = apc_fetch('test');

var_dump(current($back));
var_dump(current($back));

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
string(3) "bar"
string(3) "bar"
===DONE===
