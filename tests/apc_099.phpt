--TEST--
APCU: apcu_fetch segfault
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

apcu_store('a', 'value');
var_export(apcu_delete(['a', 'b', 'c', 0]));
--EXPECTF--
Warning: apcu_delete(): apc_delete() expects a string, array of strings, or APCIterator instance in %s on line 4
array (
  0 => 'b',
  1 => 'c',
  2 => 0,
)
