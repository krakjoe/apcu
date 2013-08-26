--TEST--
APC: APCIterator::__construct() signature compatibility
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
<?php if (!APCU_APC_FULL_BC) { die('skip compiled without APC compatibility'); } ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
$apcIterator = new APCIterator(
	'user',
	('/^hello/'),
	APC_ITER_KEY
);
?>
==DONE==
--EXPECTF--
==DONE==
