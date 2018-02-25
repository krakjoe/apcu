--TEST--
APC: APCIterator Overwriting the ctor
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
class foobar extends APCuIterator {
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
	apcu_delete($obj)
);
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
