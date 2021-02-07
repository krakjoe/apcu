--TEST--
Recursive apcu_* calls inside apcu_entry()
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

$foo = apcu_entry('foo', function() {
    apcu_store('xyz', 123);
    apcu_inc('xyz');
    var_dump(apcu_fetch('xyz'));
    return 'bar';
});
var_dump($foo);

?>
--EXPECT--
int(124)
string(3) "bar"
