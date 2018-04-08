--TEST--
apcu_inc/dec() should not inc/dec hard expired entries
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

apcu_store("inc", 0, 3);
apcu_store("dec", 0, 3);

echo "T+0:\n";

var_dump(apcu_inc("inc"));
var_dump(apcu_fetch("inc"));
var_dump(apcu_dec("dec"));
var_dump(apcu_fetch("dec"));

apcu_inc_request_time(2);
echo "T+2:\n";

var_dump(apcu_inc("inc"));
var_dump(apcu_fetch("inc"));
var_dump(apcu_dec("dec"));
var_dump(apcu_fetch("dec"));

apcu_inc_request_time(2);
echo "T+4\n";

var_dump(apcu_inc("inc"));
var_dump(apcu_fetch("inc"));
var_dump(apcu_dec("dec"));
var_dump(apcu_fetch("dec"));

?>
--EXPECT--
T+0:
int(1)
int(1)
int(-1)
int(-1)
T+2:
int(2)
int(2)
int(-2)
int(-2)
T+4
int(1)
int(1)
int(-1)
int(-1)
