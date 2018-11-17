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
--FILE--
<?php

// Reset slam detection.
apcu_store("baz", "bar");

$pid = pcntl_fork();
if ($pid) {
    // parent
    pcntl_wait($status);

    $ret = apcu_store("foo", "bar");
    if ($ret === false) {
        echo "Stampede protection works\n";
    } else {
        echo "Stampede protection doesn't work\n";
    }
} else {
    // child
    apcu_store("foo", "bar");
    exit(0);
}

?>
--EXPECT--
Stampede protection works
