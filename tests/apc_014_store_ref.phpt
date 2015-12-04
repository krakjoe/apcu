--TEST--
APC: store array of references
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=php
--FILE--
<?php
$_items = [
	'key1' => 'value1',
	'key2' => 'value2',
];
$items = [];
foreach($_items as $k => $v) {
	$items["prefix_$k"] = &$v;
}
var_dump(apcu_store($items));
?>
===DONE===
<?php exit(0); ?>
--EXPECT--
array(0) {
}
===DONE===
