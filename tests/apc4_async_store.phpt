--TEST--
APC: apc_store_async/fetch with strings
--SKIPIF--
<?php 
	require_once(dirname(__FILE__) . '/skipif.inc'); 
	if (PHP_ZTS)
		die("skip no async api in ZTS");
?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$foo = 'hello world';
var_dump($foo);
apc_store_async('foo',$foo);
usleep(100000);
$bar = apc_fetch('foo');
var_dump($bar);
$bar = 'nice';
var_dump($bar);

apc_store_async('foo\x00bar', $foo);
usleep(100000);
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
