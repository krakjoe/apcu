--TEST--
APC: apc_store/fetch with strings
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

$foo = 'hello world';
var_dump($foo);
apc_store('foo',$foo);
$bar = apc_fetch('foo');
var_dump($bar);
$bar = 'nice';
var_dump($bar);

apc_store('foo\x00bar', $foo);
$bar = apc_fetch('foo\x00bar');
var_dump($bar);

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
string(11) "hello world"
string(11) "hello world"
string(4) "nice"
string(11) "hello world"
===DONE===
