--TEST--
APCUIterator: iterate the deleted list
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=php
--FILE--
<?php
/* An entry that is deleted while a reference to it is held is moved to the
 * deleted list. apcu_fetch() holds one while unserializing, so deleting the
 * entry from __wakeup() puts it there. */
class DeleteSelf {
    public $key;
    public function __construct($key) {
        $this->key = $key;
    }
    public function __wakeup() {
        apcu_delete($this->key);
    }
}

$keys = ['a1', 'a2', 'b1', 'a3', 'b2'];
foreach ($keys as $key) {
    apcu_store($key, new DeleteSelf($key));
}
foreach ($keys as $key) {
    apcu_fetch($key);
}
var_dump(count(apcu_cache_info()['deleted_list']));

function deleted_keys($search, $chunk_size) {
    $seen = [];
    foreach (new APCUIterator($search, APC_ITER_KEY, $chunk_size, APC_LIST_DELETED) as $item) {
        $seen[] = $item['key'];
        if (count($seen) > 10) {
            echo "Endless iteration\n";
            break;
        }
    }
    sort($seen);
    return implode(',', $seen);
}

var_dump(deleted_keys(null, 0));
var_dump(deleted_keys(null, 2));
var_dump(deleted_keys('/^a/', 1));
var_dump(deleted_keys('/^a/', 2));
?>
--EXPECT--
int(5)
string(14) "a1,a2,a3,b1,b2"
string(14) "a1,a2,a3,b1,b2"
string(8) "a1,a2,a3"
string(8) "a1,a2,a3"
