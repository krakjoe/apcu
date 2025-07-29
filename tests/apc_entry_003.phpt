--TEST--
APC: apcu_entry (fatal error)
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$value = apcu_entry("test", function($key) {
    // Fatal error
    interface X { function foo(); }
    class Y implements X {}
});
?>
--EXPECTF--
Fatal error: %s
