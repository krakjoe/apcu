--TEST--
GH Bug #335: APCu stampede protection is broken
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=default
--FILE--
<?php

$pid = pcntl_fork();
if ($pid) {
    // parent
    $ret1 = apcu_store("foo", "bar");
    pcntl_wait($pid);
    $ret2 = apcu_store("foo", "bar");
    if ($ret1 === false || $ret2 === false) {
        echo "Stampede protection works\n";
    } else {
        echo "Stampede protection doesn't work\n";
    }
} else {
    // child
    apcu_store("foo", "bar");
}

?>
--EXPECT--
Stampede protection works
