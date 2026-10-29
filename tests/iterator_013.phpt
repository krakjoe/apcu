--TEST--
APCUIterator: not serializable
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php
try {
    serialize(new APCUIterator());
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
try {
    // Before PHP 8.1, only the C: format reaches the handler that throws
    unserialize((PHP_VERSION_ID < 80100 ? 'C' : 'O') . ':12:"APCUIterator":0:{}');
} catch (Exception $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Serialization of 'APCUIterator' is not allowed
Unserialization of 'APCUIterator' is not allowed
