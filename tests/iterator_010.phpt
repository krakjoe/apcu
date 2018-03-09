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

function printInfo() {
    $it = new APCuIterator();
    $keys = array_keys(iterator_to_array($it));
    sort($keys);
    var_dump($keys);
    echo "Total: {$it->getTotalCount()}\n\n";
}

apcu_store("no_ttl", "a");
apcu_store("ttl", "a", 3);

echo "Initial state:\n";
printInfo();

echo "T+2:\n";
apcu_inc_request_time(2);
printInfo();

echo "T+4:\n";
apcu_inc_request_time(2);
printInfo();

?>
--EXPECT--
Initial state:
array(2) {
  [0]=>
  string(6) "no_ttl"
  [1]=>
  string(3) "ttl"
}
Total: 2

T+2:
array(2) {
  [0]=>
  string(6) "no_ttl"
  [1]=>
  string(3) "ttl"
}
Total: 2

T+4:
array(1) {
  [0]=>
  string(6) "no_ttl"
}
Total: 1
