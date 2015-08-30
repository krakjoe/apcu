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
apc_store($key, $i);
$j=apc_fetch($key);
var_dump($i==$j);

apc_inc($key, 1);
$i++;
$j=apc_fetch($key);
var_dump($i==$j);

apc_inc($key, 1);
$i++;
$j=apc_fetch($key);
var_dump($i==$j);
?>
===DONE===
<?php exit(0); ?>
--EXPECT--
bool(true)
bool(true)
bool(true)
===DONE===
