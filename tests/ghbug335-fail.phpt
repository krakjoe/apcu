--TEST--
GH Bug #335: Test that APCu stampede protection can be disabled
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (!extension_loaded('pcntl') {
  die('skip pcntl required');
}
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.slam_defense=0
--FILE--
<?php

// Reset slam detection.
apcu_store("baz", "bar");

$pid = pcntl_fork();
if ($pid) {
    // parent
    pcntl_wait($pid);
    $ret = apcu_store("foo", "bar");
    if ($ret === false) {
        echo "Stampede protection is enabled\n";
    } else {
        echo "Stampede protection is disabled\n";
    }
} else {
    // child
    apcu_store("foo", "bar");
    exit(0);
}

?>
--EXPECT--
Stampede protection is disabled
