--TEST--
apcu_fetch should work for multiple reference groups
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$x = [];
$y = [];
$array = [&$x, &$x, &$y, &$y];
apcu_store("ary", $array);
$copy = apcu_fetch("ary");
$copy[0][1] = new stdClass();
var_dump($array);
var_dump($copy);

?>
--EXPECT--
array(4) {
  [0]=>
  &array(0) {
  }
  [1]=>
  &array(0) {
  }
  [2]=>
  &array(0) {
  }
  [3]=>
  &array(0) {
  }
}
array(4) {
  [0]=>
  &array(1) {
    [1]=>
    object(stdClass)#1 (0) {
    }
  }
  [1]=>
  &array(1) {
    [1]=>
    object(stdClass)#1 (0) {
    }
  }
  [2]=>
  &array(0) {
  }
  [3]=>
  &array(0) {
  }
}
