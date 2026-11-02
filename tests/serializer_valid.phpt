--TEST--
Serializer: valid pattern.
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=php
--FILE--
<?php
--EXPECT--
