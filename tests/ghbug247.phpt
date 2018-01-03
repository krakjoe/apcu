--TEST--
GH Bug #247: when a NUL char is used as key, apcu_fetch(array) truncates the key
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apcu_store(array("a\0b" => 'foo'));
var_dump(apcu_fetch(array("a\0b"))["a\0b"]);
?>
--EXPECT--
string(3) "foo"
