--TEST--
APC should not preserve the IAP
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=default
--FILE--
<?php

$array = [1, 2, 3];
next($array);
apcu_store("ary", $array);
$array = apcu_fetch("ary");
var_dump(current($array));

?>
--EXPECT--
int(1)
