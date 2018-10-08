--TEST--
Serialization edge cases
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$obj = new stdClass;
$obj2 = new stdClass;
$obj2->obj = $obj;
$ary = [$obj, $obj2];
apcu_store("key", $ary);
// $obj and $obj2->obj should have the same ID
var_dump(apcu_fetch("key"));

?>
--EXPECT--
array(2) {
  [0]=>
  object(stdClass)#3 (0) {
  }
  [1]=>
  object(stdClass)#4 (1) {
    ["obj"]=>
    object(stdClass)#3 (0) {
    }
  }
}
