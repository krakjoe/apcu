--TEST--
Test default expunge logic wrt global and per-entry TTLs
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

apcu_store("no_ttl_unaccessed", str_repeat('x', 500));
apcu_store("no_ttl_accessed", 24);
apcu_store("ttl", 42, 3);

apcu_inc_request_time(1);
apcu_fetch("no_ttl_accessed");

apcu_inc_request_time(1);

// Fill the cache
$i = 0;
do {
    $tmp_avail = apcu_sma_info(true)['avail_mem'];
    apcu_store("key" . $i, str_repeat('x', 500));
    $i++;
} while (apcu_sma_info(true)['avail_mem'] < $tmp_avail);

var_dump(apcu_fetch("no_ttl_unaccessed"));
var_dump(apcu_fetch("no_ttl_accessed"));
var_dump(apcu_fetch("ttl"));

?>
--EXPECT--
bool(false)
int(24)
int(42)
