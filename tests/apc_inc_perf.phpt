--TEST--
APC: apcu_inc/apcu_dec performance test (gh#164)
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php
apcu_store('foobar', 1);
$t = microtime(true);
var_dump(apcu_inc('foobar', 0x76543210));
var_dump(apcu_dec('foobar', 0x76543210));
var_dump(apcu_dec('foobar', -999999999));
var_dump(apcu_inc('foobar', -999999999));
$t = microtime(true) - $t;
var_dump($t < 0.1 ? true : $t);
?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
int(1985229329)
int(1)
int(1000000000)
int(1)
bool(true)
===DONE===
