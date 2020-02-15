--TEST--
The outermost value should always be a value, not a reference
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=default
--SKIPIF--
<?php if (PHP_VERSION_ID < 80000) die('skip Requires PHP >= 8.0.0'); ?>
--FILE--
<?php

/* The output is different for the php serializer, because it does not replicate the
 * cycle involving the top-level value. Instead the cycle is placed one level lower.
 * I believe this is a bug in the php serializer. */

$value = [&$value];
apcu_store(["key" => &$value]);
$result = apcu_fetch("key");
var_dump($result);

?>
--EXPECT--
array(1) {
  [0]=>
  *RECURSION*
}
