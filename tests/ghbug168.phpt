--TEST--
gh bug #168
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
apcu_store('prop', 'A');

var_dump($prop = apcu_fetch('prop'));

apcu_store('prop', ['B']);

var_dump(apcu_fetch('prop'), $prop);

apcu_store('thing', ['C']);

var_dump(apcu_fetch('prop'), $prop);
--EXPECT--
string(1) "A"
array(1) {
  [0]=>
  string(1) "B"
}
string(1) "A"
array(1) {
  [0]=>
  string(1) "B"
}
string(1) "A"

