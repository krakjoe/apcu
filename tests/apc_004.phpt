--TEST--
APC: apc_store/fetch with bools 
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

$foo = false;
var_dump($foo);     /* false */
apc_store('foo',$foo);
//$success = "some string";

$bar = apc_fetch('foo', $success);
var_dump($foo);     /* false */
var_dump($bar);     /* false */
var_dump($success); /* true  */

$bar = apc_fetch('not foo', $success);
var_dump($foo);     /* false */
var_dump($bar);     /* false */
var_dump($success); /* false */

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(false)
===DONE===
