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
apcu_store("foo", "parent");
apcu_store("baz", "bar");

$pid = pcntl_fork();
if ($pid) {
    // parent
    while (apcu_fetch("foo") !== "child") {
      usleep(0);
    }

    $ret = apcu_store("foo", "bar");
    if ($ret === false) {
        echo "Stampede protection works\n";
    } else {
        echo "Stampede protection doesn't work\n";
    }

    pcntl_wait($status);
} else {
    usleep(500);
    // child
    apcu_store("foo", "child");
    exit(0);
}

?>
--EXPECT--
Stampede protection works
