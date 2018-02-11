--TEST--
APC: APCIterator key invalidated between key() calls
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

apcu_store("foo", 0);
$it = new APCuIterator();
$it->rewind();
var_dump($it->key());
apcu_delete("foo");
apcu_store("bar", 0);
var_dump($it->key());

?>
--EXPECT--
string(3) "foo"
string(3) "foo"
