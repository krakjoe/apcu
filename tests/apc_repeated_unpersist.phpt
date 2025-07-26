--TEST--
APCU: Test that repeated unpersist does not break the persistence representation in SHM
--SKIPIF--
<?php
require_once(__DIR__ . '/skipif.inc');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=default
apc.shm_size=1M
--FILE--
<?php

// This test must be performed with apc.serializer=default (instead of apc.serializer=php).
// Otherwise all arrays will be persisted using the serializer, which means that not all code paths will be tested.

// store entries of different types
$tmp = [1, "a", []];
apcu_store("int", 123);
apcu_store("string", "abc");
apcu_store("object", (object) ["prop1" => "val1", "prop2" => 2]);
apcu_store("array_empty", []);
apcu_store("array_simple", $tmp);
apcu_store("array_nested", [$tmp]);
apcu_store("array_referenced", [&$tmp, &$tmp]);

echo "1st unpersist:\n";
var_dump(apcu_fetch("int") === 123);
var_dump(apcu_fetch("string") === "abc");
var_dump(apcu_fetch("object") == (object) ["prop1" => "val1", "prop2" => 2]);
var_dump(apcu_fetch("array_empty") === []);
var_dump(apcu_fetch("array_simple") === $tmp);
var_dump(apcu_fetch("array_nested") === [$tmp]);
var_dump(apcu_fetch("array_referenced") === [&$tmp, &$tmp]);

echo "2nd unpersist (check, if the 1st unpersist didn't break the representation in SHM):\n";
var_dump(apcu_fetch("int") === 123);
var_dump(apcu_fetch("string") === "abc");
var_dump(apcu_fetch("object") == (object) ["prop1" => "val1", "prop2" => 2]);
var_dump(apcu_fetch("array_empty") === []);
var_dump(apcu_fetch("array_simple") === $tmp);
var_dump(apcu_fetch("array_nested") === [$tmp]);
var_dump(apcu_fetch("array_referenced") === [&$tmp, &$tmp]);

echo "Check if the reference has been preserved:\n";
$tmp = apcu_fetch("array_referenced");
$tmp[0][0] = 2;
$tmp[0][1] = "b";
var_dump($tmp[1][0] === 2);
var_dump($tmp[1][1] === "b");

?>
--EXPECT--
1st unpersist:
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
2nd unpersist (check, if the 1st unpersist didn't break the representation in SHM):
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Check if the reference has been preserved:
bool(true)
bool(true)
