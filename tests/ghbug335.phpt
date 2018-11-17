--TEST--
GH Bug #335: APCu stampede protection is broken
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (!extension_loaded('pcntl')) {
  die('skip pcntl required');
}
die('skip Test disabled as it does not work on Travis-CI.');
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
    for ($i = 0; $i < 100; $i++) {
        // parent
        while (apcu_fetch("foo") !== "child") {
            // wait
        }

        $ret = apcu_store("foo", "parent");
        if ($ret === false) {
            echo "Stampede protection works\n";
            break;
        }
        apcu_store("baz", "bar");
    }

    if ($ret) {
      echo "Stampede protection doesn't work\n";
    }

    pcntl_wait($status);
} else {
    // child
    for ($i = 0; $i < 10000; $i++) {
      apcu_store("foo", "child");
      usleep(100);
    }
    exit(0);
}

?>
--EXPECT--
Stampede protection works
