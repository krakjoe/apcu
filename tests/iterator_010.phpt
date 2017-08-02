--TEST--
APC: APCIterator enforcement of item ttl
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
<?php if (!APCU_APC_FULL_BC) { die('skip compiled without APC compatibility'); } ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.use_request_time=0
--FILE--
<?php
apc_store("somevalue", "thedata", 1);

echo "From APCIterator before sleep: ";
$apcIterator = new APCIterator('user', null, APC_ITER_KEY);
foreach ($apcIterator as $key => $data) {
  var_dump($key);
}
echo "\nAPCIterator->getTotalCount() before sleep: " . $apcIterator->getTotalCount() . "\n";

echo "\nFrom apc_fetch before sleep: ";
var_dump(apc_fetch("somevalue"));

sleep(2);

echo "\nFrom APCIterator after sleep: ";
$apcIterator = new APCIterator('user', null, APC_ITER_KEY);
foreach ($apcIterator as $key => $data) {
  var_dump($key);
}

echo "\nAPCIterator->getTotalCount() after sleep: " . $apcIterator->getTotalCount() . "\n";

echo "\nFrom apc_fetch after sleep: ";
var_dump(apc_fetch("somevalue"));
?>
==DONE==
--EXPECTF--
From APCIterator before sleep: string(9) "somevalue"

APCIterator->getTotalCount() before sleep: 1

From apc_fetch before sleep: string(7) "thedata"

From APCIterator after sleep: 
APCIterator->getTotalCount() after sleep: 0

From apc_fetch after sleep: bool(false)
==DONE==
