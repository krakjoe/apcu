--TEST--
Store array that references same value twice
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=default
--FILE--
<?php

apcu_store('test', [&$x, &$x]);
var_dump(apcu_fetch('test'));

?>
--EXPECT--
array(2) {
  [0]=>
  &NULL
  [1]=>
  &NULL
}
