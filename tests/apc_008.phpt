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
apc_store('foobar',2);
echo "\$foobar = 2\n";
echo "\$foobar == 1 ? 2 : 1 = ".(apc_cas('foobar', 1, 2)?"ok":"fail")."\n";
echo "\$foobar == 2 ? 1 : 2 = ".(apc_cas('foobar', 2, 1)?"ok":"fail")."\n";
echo "\$foobar = ".apc_fetch("foobar")."\n";

echo "\$f__bar == 1 ? 2 : 1 = ".(apc_cas('f__bar', 1, 2)?"ok":"fail")."\n";

apc_store('perfection', "xyz");
echo "\$perfection == 2 ? 1 : 2 = ".(apc_cas('perfection', 2, 1)?"ok":"epic fail")."\n";

echo "\$foobar = ".apc_fetch("foobar")."\n";
?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
$foobar = 2
$foobar == 1 ? 2 : 1 = fail
$foobar == 2 ? 1 : 2 = ok
$foobar = 1
$f__bar == 1 ? 2 : 1 = fail
$perfection == 2 ? 1 : 2 = epic fail
$foobar = 1
===DONE===
