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
    (string) new stdClass;
});
?>
--EXPECTF--
Recoverable fatal error: Object of class stdClass could not be converted to string in %s on line %d
