--TEST--
Test SMA behavior #3
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (!function_exists('pcntl_fork')) {
    die('skip pcntl_fork not available');
}
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=32M
apc.ttl=3600
--FILE--
<?php

// check infinite loop

$num_of_child = 10;
$num_of_store = 1000;
$value = str_repeat('X', 1024 * 300); // 300KB

for ($i = 0; $i < $num_of_child; $i++) {
    $pid = pcntl_fork();
    if ($pid === -1) {
        die('failed to fork');
    } else if ($pid) {
        // parent
    } else {
        // child
        for ($j = 0; $j < $num_of_store; $j++) {
            $key = "test$i-$j";
            /*
             * When using TTL, only expired entries may be deleted on default expunge.
             * In such cases, it may cause an allocation failure when multiple processes try to store simultaneously.
             * So, failures are ignored.
             */
            apcu_store($key, $value);
        }
        exit(0);
    }
}
?>
--EXPECT--
