--TEST--
APC: apcu_exists_any
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$kyes = "testkey";
$kno  = "keytest";
apcu_store($kyes, 1);
var_dump(apcu_exists_any([]));
var_dump(apcu_exists_any([$kyes]));
var_dump(apcu_exists_any([$kno]));
var_dump(apcu_exists_any([$kyes, $kno]));
?>
===DONE===
--EXPECT--
bool(false)
bool(true)
bool(false)
bool(true)
===DONE===
