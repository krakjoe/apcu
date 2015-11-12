--TEST--
APC: apcu_entry (recursion)
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$value = apcu_entry("test", function($key){
	return apcu_entry("child", function($key) {
		return "Hello World";
	});
});

var_dump($value, apcu_entry("test", function($key){
	return "broken";
}), apcu_entry("child", function(){
	return "broken";
}));
?>
===DONE===
<?php exit(0); ?>
--EXPECT--
string(11) "Hello World"
string(11) "Hello World"
string(11) "Hello World"
===DONE===
