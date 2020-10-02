--TEST--
Behavior of functions when apcu is disabled
--INI--
apc.enable_cli=0
--FILE--
<?php

echo "enabled\n";
var_dump(apcu_enabled());

echo "\nclear_cache\n";
var_dump(apcu_clear_cache());

echo "\ncache/sma_info\n";
var_dump(apcu_cache_info());
var_dump(apcu_sma_info());

echo "\nstore/add/exists/fetch/key_info/delete\n";
var_dump(apcu_store("key", "value"));
var_dump(apcu_add("key", "value"));
var_dump(apcu_exists("key"));
var_dump(apcu_fetch("key"));
var_dump(apcu_key_info("key"));
var_dump(apcu_delete("key"));

echo "\nstore/add/exists/fetch/delete array\n";
var_dump(apcu_store(["key" => "value"]));
var_dump(apcu_add(["key" => "value"]));
var_dump(apcu_exists(["key"]));
var_dump(apcu_fetch(["key"]));
var_dump(apcu_delete(["key"]));

echo "\ninc/dec/cas\n";
var_dump(apcu_inc("key", 1, $succ_inc));
var_dump($succ_inc);
var_dump(apcu_dec("key", 1, $succ_dec));
var_dump($succ_dec);
var_dump(apcu_cas("key", 10, 20));

echo "\nentry\n";
var_dump(apcu_entry("key", function() { return 42; }));

echo "\niterator\n";
try {
    new APCUIterator;
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECTF--
enabled
bool(false)

clear_cache
bool(true)

cache/sma_info

Warning: apcu_cache_info(): No APC info available.  Perhaps APC is not enabled? Check apc.enabled in your ini file in %s on line %d
bool(false)

Warning: apcu_sma_info(): No APC SMA info available.  Perhaps APC is disabled via apc.enabled? in %s on line %d
bool(false)

store/add/exists/fetch/key_info/delete
bool(false)
bool(false)
bool(false)
bool(false)
NULL
bool(false)

store/add/exists/fetch/delete array
array(1) {
  ["key"]=>
  int(-1)
}
array(1) {
  ["key"]=>
  int(-1)
}
array(0) {
}
array(0) {
}
array(1) {
  [0]=>
  string(3) "key"
}

inc/dec/cas
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)

entry
NULL

iterator
APC must be enabled to use APCUIterator
