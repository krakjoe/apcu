--TEST--
Huge allocation attempts which don't fit into shm shouldn't cause cache wipes
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=1M
--FILE--
<?php

// create a small entry which should survive
apcu_store("test_1", 123);

// try to store an entry which is larger than the shm size (this should fail)
var_dump(apcu_store("large_entry", str_repeat('x', 1024 * 1024)));

// fetch the small entry and check if it's still there
var_dump(apcu_fetch("test_1") === 123);
?>
--EXPECT--
bool(false)
bool(true)
