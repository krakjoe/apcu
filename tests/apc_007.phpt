--TEST--
APC: apc_inc/apc_dec test
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php
apc_store('foobar',2);
echo "\$foobar = 2 \n";
echo "\$foobar += 1 = ".apc_inc('foobar')."\n";
echo "\$foobar += 10 = ".apc_inc('foobar', 10)."\n";

echo "\$foobar -= 1 = ".apc_dec('foobar')."\n";
echo "\$foobar -= 10 = ".apc_dec('foobar',10)."\n";

echo "\$f__bar += 1 = ".(apc_inc('f__bar')?"ok":"fail")."\n";

apc_store('perfection', "xyz");
echo "\$perfection -= 1 = ".(apc_inc('perfection')?"ok":"epic fail")."\n";

$success = false;

echo "\$foobar += 1 = ".apc_inc('foobar', 1, $success)."\n";
echo "pass by ref success ". $success . "\n";
echo "\$foobar -= 1 = ".apc_dec('foobar', 1, $success)."\n";
echo "pass by ref success ". $success . "\n";

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
$foobar = 2 
$foobar += 1 = 3
$foobar += 10 = 13
$foobar -= 1 = 12
$foobar -= 10 = 2
$f__bar += 1 = fail
$perfection -= 1 = epic fail
$foobar += 1 = 3
pass by ref success 1
$foobar -= 1 = 2
pass by ref success 1
===DONE===
