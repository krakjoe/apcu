--TEST--
The same string is used as the cache key and an array key
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=default
--FILE--
<?php

$key = 'key';
$a = [$key => null];
apcu_store($key, $a);

?>
--EXPECT--
