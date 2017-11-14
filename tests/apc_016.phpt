--TEST--
Should be able to pass references to strings to apcu_fetch
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$array = ['foo', 'bar'];
var_dump(apcu_store('foo', 'baz'));
var_dump(apcu_fetch($array));
var_dump(error_get_last());
array_walk($array, function(&$item) {});
var_dump($array);
var_dump(apcu_fetch($array));
var_dump(error_get_last());
?>
--EXPECT--
bool(true)
array(1) {
  ["foo"]=>
  string(3) "baz"
}
NULL
array(2) {
  [0]=>
  string(3) "foo"
  [1]=>
  string(3) "bar"
}
array(1) {
  ["foo"]=>
  string(3) "baz"
}
NULL
