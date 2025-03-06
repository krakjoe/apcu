--TEST--
Test expiration of entries when > 50% of memory is available
--SKIPIF--
<?php
require_once(__DIR__ . '/skipif.inc');
if (!function_exists('apcu_inc_request_time')) die('skip APC debug build required');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.use_request_time=1
apc.ttl=1
apc.shm_size=1M
--FILE--
<?php


apcu_store("key_01", 123, 100);
apcu_store("key_02", str_repeat('x', 400000), 1);

apcu_inc_request_time(2);

var_dump(apcu_store("key_03", str_repeat('x', 900000), 1));
var_dump(apcu_fetch("key_01"));
var_dump(apcu_fetch("key_02"));
var_dump(apcu_fetch("key_03") === str_repeat('x', 900000));

?>
--EXPECT--
bool(true)
int(123)
bool(false)
bool(true)
