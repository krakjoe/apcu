--TEST--
Using empty string as key
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

var_dump(apcu_store("", 123));
var_dump(apcu_exists(""));
var_dump(apcu_fetch(""));
var_dump(apcu_delete(""));

?>
--EXPECT--
bool(true)
bool(true)
int(123)
bool(true)
