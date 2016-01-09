--TEST--
APC: integer overflow consistency
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$key="testkey";
$i=PHP_INT_MAX;
apcu_store($key, $i);
var_dump($j=apcu_fetch($key));
var_dump($i==$j);

apcu_inc($key, 1);
$i++;
var_dump($j=apcu_fetch($key));
var_dump($i==$j);

$i=PHP_INT_MIN;
apcu_store($key, $i);
apcu_dec($key, 1);
$i--;
var_dump($j=apcu_fetch($key));
var_dump($i==$j);
?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
int(%d)
bool(true)
float(%s)
bool(true)
float(%s)
bool(true)
===DONE===
