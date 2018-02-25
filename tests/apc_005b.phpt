--TEST--
APC: apcu_store/fetch with arrays with duplicate object
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$o = new stdClass();
$foo = array($o, $o);

var_dump($foo);

apcu_store('foo',$foo);

$bar = apcu_fetch('foo');
var_dump($foo);
// $bar[0] should be identical to $bar[1], and not a reference
var_dump($bar);
?>
===DONE===
--EXPECT--
array(2) {
  [0]=>
  object(stdClass)#1 (0) {
  }
  [1]=>
  object(stdClass)#1 (0) {
  }
}
array(2) {
  [0]=>
  object(stdClass)#1 (0) {
  }
  [1]=>
  object(stdClass)#1 (0) {
  }
}
array(2) {
  [0]=>
  object(stdClass)#2 (0) {
  }
  [1]=>
  object(stdClass)#2 (0) {
  }
}
===DONE===
