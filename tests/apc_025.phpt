--TEST--
APC: apcu_fetch of packed array
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
var_dump(apcu_store('foo', [1, 2, 3, 4, 5, 6, 7, 8]));
var_dump(apcu_fetch('foo'));

?>
--EXPECT--
bool(true)
array(8) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
  [5]=>
  int(6)
  [6]=>
  int(7)
  [7]=>
  int(8)
}
