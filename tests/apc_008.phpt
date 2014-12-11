--TEST--
APC: apc_cas test
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

echo "String:\n";

echo "apc_cas('str', 'one', 'two'); ";
var_dump(apc_cas('str', 'one', 'two'));

echo "apc_store('str', 'two'); ";
var_dump(apc_store('str', 'two'));

echo "apc_cas('str', 'one', 'three'); ";
var_dump(apc_cas('str', 'one', 'three'));

echo "apc_cas('str', 'two', 'three'); ";
var_dump(apc_cas('str', 'two', 'three'));

echo "apc_fetch('str'); ";
var_dump(apc_fetch('str'));

echo "Integers: \n";

echo "apc_cas('int', 123, 456); ";
var_dump(apc_cas('int', 123, 456));

echo "apc_store('int', 2); ";
var_dump(apc_store('int', 2));

echo "apc_cas('int', 1, 2); ";
var_dump(apc_cas('int', 1, 2));

echo "apc_cas('int', 2, 1); ";
var_dump(apc_cas('int', 2, 1));

echo "apc_fetch('int'); ";
var_dump(apc_fetch('int'));

echo "Float:\n";

echo "apc_cas('float', 1.23, 4.56); ";
var_dump(apc_cas('float', 1.23, 4.56));

echo "apc_store('float', 3.45); ";
var_dump(apc_store('float', 3.45));

echo "apc_cas('float', 9.99, 10.0); ";
var_dump(apc_cas('float', 9.99, 10.0));

echo "apc_cas('float', 3.45, 6.78); ";
var_dump(apc_cas('float', 3.45, 6.78));

echo "apc_fetch('float'); ";
var_dump(apc_fetch('float'));

echo "Bool:\n";

echo "apc_cas('bool', true, false); ";
var_dump(apc_cas('bool', true, false));

echo "apc_store('bool', true); ";
var_dump(apc_store('bool', true));

echo "apc_cas('bool', false, true); ";
var_dump(apc_cas('bool', false, true));

echo "apc_cas('bool', true, false); ";
var_dump(apc_cas('bool', true, false));

echo "apc_fetch('bool'); ";
var_dump(apc_fetch('bool'));

echo "NULL:\n";

echo "apc_cas('null', null, null); ";
var_dump(apc_cas('null', null, null));

echo "apc_store('null', null); ";
var_dump(apc_store('null', null));

echo "apc_cas('null', null, null); ";
var_dump(apc_cas('null', null, null));

echo "apc_cas('null', null, null); ";
var_dump(apc_cas('null', null, null));

echo "apc_fetch('null'); ";
var_dump(apc_fetch('null'));

// Currently a value can only be replaces by a scalar
echo "Replace with non scalar: \n";

echo "apc_store('scalar', 'scalar'); ";
var_dump(apc_store('scalar', 'scalar'));

echo "apc_cas('scalar', 'scalar', array()); ";
var_dump(apc_cas('scalar', 'scalar', array()));

echo "apc_cas('scalar', 'scalar', new stdClass); ";
var_dump(apc_cas('scalar', 'scalar', new stdClass));

echo "apc_fetch('scalar'); ";
var_dump(apc_fetch('scalar'));

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
String:
apc_cas('str', 'one', 'two'); bool(false)
apc_store('str', 'two'); bool(true)
apc_cas('str', 'one', 'three'); bool(false)
apc_cas('str', 'two', 'three'); bool(true)
apc_fetch('str'); string(5) "three"
Integers: 
apc_cas('int', 123, 456); bool(false)
apc_store('int', 2); bool(true)
apc_cas('int', 1, 2); bool(false)
apc_cas('int', 2, 1); bool(true)
apc_fetch('int'); int(1)
Float:
apc_cas('float', 1.23, 4.56); bool(false)
apc_store('float', 3.45); bool(true)
apc_cas('float', 9.99, 10.0); bool(false)
apc_cas('float', 3.45, 6.78); bool(true)
apc_fetch('float'); float(6.78)
Bool:
apc_cas('bool', true, false); bool(false)
apc_store('bool', true); bool(true)
apc_cas('bool', false, true); bool(false)
apc_cas('bool', true, false); bool(true)
apc_fetch('bool'); bool(false)
NULL:
apc_cas('null', null, null); bool(false)
apc_store('null', null); bool(true)
apc_cas('null', null, null); bool(true)
apc_cas('null', null, null); bool(true)
apc_fetch('null'); NULL
Replace with non scalar: 
apc_store('scalar', 'scalar'); bool(true)
apc_cas('scalar', 'scalar', array()); bool(false)
apc_cas('scalar', 'scalar', new stdClass); bool(false)
apc_fetch('scalar'); string(6) "scalar"
===DONE===
