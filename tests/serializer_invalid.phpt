--TEST--
Serializer: invalid pattern.
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=igbinary
--FILE--
<?php
--EXPECT--
Warning: Unknown: apc_cache_serializer: serializer "igbinary" is not supported. in Unknown on line 0
