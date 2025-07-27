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
apc.shm_size=1M
--FILE--
<?php

// fill_cache() fills the cache with small entries with alternating ttl=1 and ttl=3
function fill_cache(): int {
    $i = 0;
    $entry_size = apcu_sma_info(true)['avail_mem'];
    apcu_store(sprintf("ttl1_%010d", $i), $i, 1);
    $entry_size -= apcu_sma_info(true)['avail_mem'];

    while (apcu_sma_info(true)['avail_mem'] >= $entry_size) {
        $i++;
        apcu_store(sprintf("ttl3_%010d", $i), $i, 3);

        if (apcu_sma_info(true)['avail_mem'] >= $entry_size) {
            apcu_store(sprintf("ttl1_%010d", $i), $i, 1);
        }
    }

    return $i;
}

// store the first entry with ttl=1, which causes all entries
// behind this entry to be moved during defragmentation
apcu_store("ttl1_int", 123, 1);

// store entries of different datatypes with ttl=3, which must be present after expiration + defragmentation
$reference = "b";
apcu_store("ttl3_int", 123456789, 3);
apcu_store("ttl3_string", "abc", 3);
apcu_store("ttl3_array", [1, "a", &$reference, []], 3);
apcu_store("ttl3_object", (object) ["prop1" => "val1", "prop2" => 2], 3);

// safe available memory for later comparison
$avail_before_filled = apcu_sma_info(true)['avail_mem'];

// fill cache with alternating ttl=1 + ttl=3 entries
fill_cache();

// expire all ttl1_* entries
apcu_inc_request_time(2);

// this insertion should trigger an default_expunge which removes all ttl1_ entries and performs a defragmentation
var_dump(apcu_store("large_entry", str_repeat('x', 1000), 1));

// delete large_entry to be able to check the available memory in the next step
var_dump(apcu_delete("large_entry"));

// the defragmentation should have freed more than 50% of the filled memory, because "ttl1_int" was also freed
var_dump(apcu_sma_info(true)['avail_mem'] > $avail_before_filled / 2);

// after the default expunge, all ttl1_ entries should not be present anymore
var_dump(apcu_fetch("ttl1_int") === false);

// all ttl3_ entries must be present (and correct after defragmentation)
var_dump(apcu_fetch("ttl3_int") === 123456789);
var_dump(apcu_fetch("ttl3_string") === "abc");
var_dump(apcu_fetch("ttl3_array") === [1, "a", &$reference, []]);
var_dump(apcu_fetch("ttl3_object") == (object) ["prop1" => "val1", "prop2" => 2]);

// check that cache cleanup and defragmentation have been performed, but no real expunge
var_dump(apcu_cache_info(true)["cleanups"] === 1);
var_dump(apcu_cache_info(true)["defragmentations"] === 1);
var_dump(apcu_cache_info(true)["expunges"] === 0);

// this insertion should trigger an default_expunge which performs a real expunge (but no defragmentation)
var_dump(apcu_store("huge_entry", str_repeat('x', 700000), 1));

// check that cache cleanup and real expunge have been performed, but no defragmentation
var_dump(apcu_cache_info(true)["cleanups"] === 2);
var_dump(apcu_cache_info(true)["defragmentations"] === 1);
var_dump(apcu_cache_info(true)["expunges"] === 1);

?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
