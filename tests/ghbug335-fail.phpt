--TEST--
GH Bug #335: APCu stampede protection is broken
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (!extension_loaded('pcntl')) {
  die('skip pcntl required');
}
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.use_request_time=1
apc.slam_defense=0
--FILE--
<?php

// Reset slam detection.
apcu_store("foo", "parent");

$pid = pcntl_fork();
if ($pid) {
    // parent
    pcntl_wait($status);
} else {
    // child
    $ret = apcu_store("foo", "child");
    if ($ret === false) {
        echo "Stampede protection works\n";
    } else {
        echo "Stampede protection doesn't work\n";
    }
    exit(0);
}

?>
--EXPECT--
Stampede protection doesn't work
