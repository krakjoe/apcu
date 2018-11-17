--TEST--
GH Bug #335: APCu stampede protection is broken
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
if (!extension_loaded('pcntl') die('skip pcntl required');
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$pid = pcntl_fork();
if ($pid) {
    // parent
    pcntl_wait($pid);
    $ret1 = apcu_store("foo", "bar");
    $ret2 = apcu_store("foo", "bar");
    if ($ret1 === false || $ret2 === false) {
        echo "Stampede protection works\n";
    } else {
        echo "Stampede protection doesn't work\n";
    }
} else {
    // child
    for ($i = 0; $i < 1000; $i++) {
      apcu_store("foo", "bar");
    }
    exit(0);
}

?>
--EXPECT--
Stampede protection works
