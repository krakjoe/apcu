--TEST--
APCUIterator: re-entrant apcu access from __wakeup during value iteration
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=php
--FILE--
<?php
/* The iterator must not hold the cache lock while unserializing a value, because
 * unserialization runs userland code (__wakeup) that can re-enter apcu. Before the
 * fix this deadlocked the worker. */
class Reenter {
    public $v = 1;
    public function __wakeup() {
        apcu_store('reentered', 2);
    }
}

apcu_store('obj', new Reenter());

$seen = [];
foreach (new APCUIterator(null, APC_ITER_KEY | APC_ITER_VALUE) as $item) {
    $seen[$item['key']] = true;
}

var_dump(isset($seen['obj']));
var_dump(apcu_fetch('reentered'));
echo "DONE\n";
?>
--EXPECT--
bool(true)
int(2)
DONE
