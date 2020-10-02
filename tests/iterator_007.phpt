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
try {
    $obj->rewind();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->current();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->key();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->next();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->valid();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->getTotalHits();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->getTotalSize();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    $obj->getTotalCount();
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
try {
    apcu_delete($obj);
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
Trying to use uninitialized APCUIterator
