--TEST--
APC: APCIterator shows entries older than global TTL
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (!function_exists('apcu_inc_request_time')) die('skip APC debug build required');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.use_request_time=1
apc.ttl=1
--FILE--
<?php

function printKeys() {
    $keys = array_keys(iterator_to_array(new APCuIterator()));
    sort($keys);
    var_dump($keys);
}

apcu_store("no_ttl", "a");
apcu_store("ttl", "a", 3);

echo "Initial state:\n";
printKeys();

echo "T+2:\n";
apcu_inc_request_time(2);
printKeys();

echo "T+4:\n";
apcu_inc_request_time(2);
printKeys();

?>
--EXPECT--
Initial state:
array(2) {
  [0]=>
  string(6) "no_ttl"
  [1]=>
  string(3) "ttl"
}
T+2:
array(2) {
  [0]=>
  string(6) "no_ttl"
  [1]=>
  string(3) "ttl"
}
T+4:
array(1) {
  [0]=>
  string(6) "no_ttl"
}
