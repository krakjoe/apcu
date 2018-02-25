--TEST--
APC: apcu_fetch resets array pointers
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$items = array('bar', 'baz');

apcu_store('test', $items);

$back = apcu_fetch('test');

var_dump(current($back));
var_dump(current($back));

?>
===DONE===
--EXPECT--
string(3) "bar"
string(3) "bar"
===DONE===
