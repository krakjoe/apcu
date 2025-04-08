--TEST--
APC: APCIterator throws for invalid flags
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (PHP_INT_SIZE == 4) echo "skip 64-bit only\n";
?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
foreach ([[null, -1], [null, PHP_INT_MAX], ['/invalidRegex'], [null, 0, -1]] as $args) {
    try {
        var_dump(new APCuIterator(...$args));
    } catch (Throwable $e) {
        printf("Caught %s: %s\n", get_class($e), $e->getMessage());
    }
}
foreach ([[[]], 4.2, new stdClass()] as $delete) {
    try {
        apcu_delete($delete);
    } catch (Throwable $e) {
        printf("Caught %s: %s\n", get_class($e), $e->getMessage());
    }
}
?>
--EXPECTF--
Caught Error: APCUIterator format is invalid
Caught Error: APCUIterator format is invalid

Warning: APCUIterator::__construct(): No ending delimiter '/' found in %s on line 4
Caught Error: Could not compile regular expression: /invalidRegex
Caught Error: APCUIterator chunk size must be 0 or greater

Warning: apcu_delete(): apcu_delete() expects a string, array of strings, or APCUIterator instance in %s on line 11

Warning: apcu_delete(): apcu_delete() expects a string, array of strings, or APCUIterator instance in %s on line 11

Warning: apcu_delete(): apcu_delete object argument must be an instance of APCUIterator. in %s on line 11
