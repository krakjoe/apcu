--TEST--
APC: apcu_store/fetch with arrays of objects
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$foo = array(new stdclass(), new stdclass());

var_dump($foo);

apcu_store('foo',$foo);

$bar = apcu_fetch('foo');
var_dump($foo);
var_dump($bar);

?>
===DONE===
--EXPECT--
array(2) {
  [0]=>
  object(stdClass)#1 (0) {
  }
  [1]=>
  object(stdClass)#2 (0) {
  }
}
array(2) {
  [0]=>
  object(stdClass)#1 (0) {
  }
  [1]=>
  object(stdClass)#2 (0) {
  }
}
array(2) {
  [0]=>
  object(stdClass)#3 (0) {
  }
  [1]=>
  object(stdClass)#4 (0) {
  }
}
===DONE===
