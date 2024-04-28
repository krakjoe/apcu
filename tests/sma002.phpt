--TEST--
Test SMA behavior #2
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
--FILE--
<?php

// see: apc_sma_malloc_ex of apc_sma.c

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
            $success = apcu_store($key, $value);
            if (!$success) {
                die("apcu_store failed\n");
            }
        }
        exit(0);
    }
}
?>
--EXPECT--
