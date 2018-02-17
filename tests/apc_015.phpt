--TEST--
Copy failure should not create entry
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--FILE--
<?php
try {
	apcu_store('thing', function(){});
} catch(Exception $ex) {
}

var_dump(apcu_exists('thing'));
--EXPECT--
bool(false)
