--TEST--
Test defragmentation
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

// Fill the cache with small entries
function fill_cache_worstcase(): int {
    $i = 0;
    $min_entry_size = apcu_sma_info(true)['avail_mem'];
    apcu_store(sprintf("ttl1_%010d", $i), $i, 1);
    $min_entry_size -= apcu_sma_info(true)['avail_mem'];

    while (apcu_sma_info(true)['avail_mem'] > $min_entry_size) {
        $i++;
        apcu_store(sprintf("ttl3_%010d", $i), $i, 3);

        if (apcu_sma_info(true)['avail_mem'] > $min_entry_size) {
            apcu_store(sprintf("ttl1_%010d", $i), $i, 1);
        }
    }

    return $i;
}

// store a long-living entry
apcu_store("long_ttl", 123456789, 100);

// fill cache with alternating ttl1 + ttl3 entries
fill_cache_worstcase();

// ensure that cache is full
var_dump(apcu_sma_info(true)['avail_mem']);

// expire all ttl1_* entries
apcu_inc_request_time(2);

// This insertion should trigger an default_expunge which removes all ttl1_* entries and performs a defragmentation
apcu_store("large_entry", str_repeat('x', 10000), 1);

// after the default expunge, all ttl1 entries should not be present anymore
var_dump(apcu_fetch("ttl1_0000000001") === false);

// all other entries must be present
var_dump(apcu_fetch("ttl3_0000000001") === 1);
var_dump(apcu_fetch("long_ttl") === 123456789);
var_dump(apcu_fetch("large_entry") === str_repeat('x', 10000));

?>
--EXPECT--
float(0)
bool(true)
bool(true)
bool(true)
bool(true)
