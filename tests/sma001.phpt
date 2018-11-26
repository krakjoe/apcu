--TEST--
Test SMA behavior #1
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=16M
--FILE--
<?php

// Make sure that a sequence of alternating small and large
// allocations does not result in catastrophic fragmentation

$len = 1024 * 1024;
for ($i = 0; $i < 100; $i++) {
    apcu_delete("key");
    $result = apcu_store("key", str_repeat("x", $len));
    if ($result === false) {
        echo "Failed $i.\n";
    }

    // Force a small allocation
    apcu_store("dummy" . $i, null);

    // Increase $len slightly
    $len += 100;
}

?>
===DONE===
--EXPECT--
===DONE===
