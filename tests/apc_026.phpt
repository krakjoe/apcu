--TEST--
Test TTL expiration with fragmented memory
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

apcu_store("long_ttl", 123456789, 100);
apcu_store("dummy", str_repeat('x', 11000));
apcu_inc_request_time(2);

// Fill the cache
$i = 1;
while (apcu_exists("dummy")) {
    apcu_store("key_small_" . $i, $i, 3);
    apcu_store("key_large_" . $i, str_repeat('x', 10000), 1);

    $i++;
}

// Expire all large entries
apcu_inc_request_time(2);

// This insertion should trigger an expunge which expires all large entries
var_dump(apcu_store("next_large_entry", str_repeat('x', 10000), 1));
var_dump(apcu_fetch("next_large_entry") === str_repeat('x', 10000));

// Check fragmented state (small entries are present, large entries are not present)
var_dump(apcu_fetch("key_small_1"));
var_dump(apcu_fetch("key_large_1"));

// Expire all small entries
apcu_inc_request_time(2);

// This entry should trigger an expunge which expires all small entries.
// This solves fragmentation which allows this entry to be stored without a full cache wipe.
var_dump(apcu_store("very_large_entry", str_repeat('x', 20000), 1));
var_dump(apcu_fetch("very_large_entry") === str_repeat('x', 20000));
var_dump(apcu_fetch("long_ttl"));

?>
--EXPECT--
bool(true)
bool(true)
int(1)
bool(false)
bool(true)
bool(true)
int(123456789)
