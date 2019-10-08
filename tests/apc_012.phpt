--TEST--
APC: Atomic inc + dec wrap around on overflow
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$key = "testkey";

apcu_store($key, PHP_INT_MAX);
var_dump($i = apcu_inc($key, 1));
var_dump($j = apcu_fetch($key));
var_dump($i == $j);
var_dump($j == PHP_INT_MIN);

apcu_store($key, PHP_INT_MIN);
var_dump($i = apcu_dec($key, 1));
var_dump($j = apcu_fetch($key));
var_dump($i == $j);
var_dump($j == PHP_INT_MAX);

?>
===DONE===
--EXPECTF--
int(%i)
int(%i)
bool(true)
bool(true)
int(%i)
int(%i)
bool(true)
bool(true)
===DONE===
