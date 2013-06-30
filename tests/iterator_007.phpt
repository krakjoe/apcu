--TEST--
APC: APCIterator Overwriting the ctor
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
<?php if (APCU_APC_FULL_BC) { die('skip compiled with APC compatibility'); } ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
class foobar extends APCIterator {
	public function __construct() {}
}
$obj = new foobar;
var_dump(
	$obj->rewind(),
	$obj->current(),
	$obj->key(),
	$obj->next(),
	$obj->valid(),
	$obj->getTotalHits(),
	$obj->getTotalSize(),
	$obj->getTotalCount(),
	apc_delete($obj)
);
?>
--EXPECTF--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)

