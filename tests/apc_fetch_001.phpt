--TEST--
APC: apcu_store/fetch with NUL characters will be escaped
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

apcu_store(array("a\0b" => 'foo'));
var_dump(apcu_fetch(array("a\0b")));

?>
===DONE===
<?php exit(0); ?>
--EXPECT--
array(1) {
  ["a\0b"]=>
  string(3) "foo"
}
===DONE===
