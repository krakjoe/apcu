--TEST--
The per-entry TTL should take precedence over the global TTL
--SKIPIF--
<?php
require_once(__DIR__ . '/skipif.inc');
if (!function_exists('apcu_inc_request_time')) die('skip APC debug build required');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.use_request_time=1
apc.ttl=1
--FILE--
<?php

/* Keys chosen to collide */
apcu_store("EzEz", 24);
apcu_store("EzFY", 42, 3);
apcu_store("FYEz", "xxx");

echo "T+2\n";
apcu_inc_request_time(2);
apcu_store("FYEz", "xxx");
var_dump(apcu_fetch("EzEz"));
var_dump(apcu_fetch("EzFY"));

echo "T+4\n";
apcu_inc_request_time(2);
apcu_store("FYEz", "xxx");
var_dump(apcu_fetch("EzEz"));
var_dump(apcu_fetch("EzFY"));

?>
--EXPECT--
T+2
bool(false)
int(42)
T+4
bool(false)
bool(false)
