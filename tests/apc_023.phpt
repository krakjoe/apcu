--TEST--
Serialization edge cases
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

echo "GLOBALS:\n";
$foo = 1;
apcu_store("key", $GLOBALS);
$globals = apcu_fetch("key");
var_dump($globals['foo']);

echo "Object referential identity:\n";
$obj = new stdClass;
$obj2 = new stdClass;
$obj2->obj = $obj;
$ary = [$obj, $obj2];
apcu_store("key", $ary);
// $obj and $obj2->obj should have the same ID
var_dump(apcu_fetch("key"));

echo "Array next free element:\n";
$ary = [0, 1];
unset($ary[1]);
apcu_store("key", $ary);
$ary = apcu_fetch("key");
// This should use key 1 rather than 2, as
// nextFreeElement should not be preserved (serialization does not)
$ary[] = 1;
var_dump($ary);

echo "Resources:\n";
apcu_store("key", fopen(__FILE__, "r"));

?>
--EXPECTF--
GLOBALS:
int(1)
Object referential identity:
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
Array next free element:
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(1)
}
Resources:

Warning: apcu_store(): Cannot store resources in apcu cache in %s on line %d
