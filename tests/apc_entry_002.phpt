--TEST--
APC: apcu_entry (exception)
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$value = apcu_entry("test", function($key){
	throw new Exception($key);
});
?>
--EXPECTF--
Fatal error: Uncaught Exception: test in %s:3
Stack trace:
#0 [internal function]: {closure}('test')
#1 %s(4): apcu_entry('test', Object(Closure))
#2 {main}
  thrown in %s on line 3
