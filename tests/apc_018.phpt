--TEST--
apc_cas() inserts non-existing key with value 0
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

apcu_cas("foo", 42, 24);
var_dump(apcu_fetch("foo"));

?>
--EXPECT--
bool(false)
