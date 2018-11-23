--TEST--
The outermost value should always be a value, not a reference
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$value = [&$value];
apcu_store(["key" => &$value]);
$result = apcu_fetch("key");
var_dump($result);

?>
--EXPECT--
array(1) {
  [0]=>
  array(1) {
    [0]=>
    *RECURSION*
  }
}
