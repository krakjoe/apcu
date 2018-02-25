--TEST--
APC: apcu_store/fetch with bools
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$foo = false;
var_dump($foo);     /* false */
apcu_store('foo',$foo);
//$success = "some string";

$bar = apcu_fetch('foo', $success);
var_dump($foo);     /* false */
var_dump($bar);     /* false */
var_dump($success); /* true  */

$bar = apcu_fetch('not foo', $success);
var_dump($foo);     /* false */
var_dump($bar);     /* false */
var_dump($success); /* false */

?>
===DONE===
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
===DONE===
