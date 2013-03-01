--TEST--
APC: apc_store/fetch/add with array of key/value pairs.
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

$entries = array();
$entries['key1'] = 'value1';
$entries['key2'] = 'value2';
$entries['key3'] = array('value3a','value3b');
$entries['key4'] = 4;

var_dump(apc_store($entries));
$cached_values = apc_fetch(array_keys($entries));
var_dump($cached_values);

apc_delete('key2');
apc_delete('key4');
$cached_values = apc_fetch(array_keys($entries));
var_dump($cached_values);
var_dump(apc_add($entries));
$cached_values = apc_fetch(array_keys($entries));
var_dump($cached_values);

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
array(0) {
}
array(4) {
  ["key1"]=>
  string(6) "value1"
  ["key2"]=>
  string(6) "value2"
  ["key3"]=>
  array(2) {
    [0]=>
    string(7) "value3a"
    [1]=>
    string(7) "value3b"
  }
  ["key4"]=>
  int(4)
}
array(2) {
  ["key1"]=>
  string(6) "value1"
  ["key3"]=>
  array(2) {
    [0]=>
    string(7) "value3a"
    [1]=>
    string(7) "value3b"
  }
}
array(2) {
  ["key1"]=>
  int(-1)
  ["key3"]=>
  int(-1)
}
array(4) {
  ["key1"]=>
  string(6) "value1"
  ["key2"]=>
  string(6) "value2"
  ["key3"]=>
  array(2) {
    [0]=>
    string(7) "value3a"
    [1]=>
    string(7) "value3b"
  }
  ["key4"]=>
  int(4)
}
===DONE===
