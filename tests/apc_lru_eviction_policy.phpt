--TEST--
APC: LRU eviction policy
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=1M
apc.entries_hint=4096
apc.eviction_policy=lru
--FILE--
<?php
$value = str_repeat('X', 1024 * 300); // 300KB

apcu_store('A', $value); // A
echo keylist();
apcu_add('B', $value); // B->A
echo keylist();
apcu_add('A', $value); // B->A (no effect)
echo keylist();
apcu_store('C', $value); // C->B->A
echo keylist();
apcu_fetch('A'); // A->C->B
echo keylist();
apcu_exists('B'); // A->C->B (no effect)
echo keylist();
apcu_store(['D' => $value, 'E' => $value]); // E->D->A
echo keylist();
apcu_entry('A', function ($key) use ($value) { return $value; }); // A->E->D
echo keylist();
apcu_entry('F', function ($key) use ($value) { return $value; }); // F->A->E
echo keylist();
apcu_fetch(['F', 'A']); // A->F->E
echo keylist();
apcu_delete('A'); // F->E
echo keylist();
apcu_delete('E'); // F
echo keylist();
apcu_store(['G' => $value, 'H' => $value]); // H->G->F
echo keylist();
apcu_store('I', str_repeat($value, 2)); // I->H
echo keylist();
apcu_clear_cache();
echo keylist();
apcu_store([
    'A' => 1,
    'B' => 2,
    'C' => 3,
    'D' => 4,
    'E' => $value,
    'F' => $value,
    'G' => $value,
]); // G->F->E->D->C->B->A
echo keylist();
apcu_inc('A'); // A->G->F->E->D->C->B
echo keylist();
apcu_dec('B'); // B->A->G->F->E->D->C
echo keylist();
apcu_cas('C', 3, 5); // C->B->A->G->F->E->D
echo keylist();
apcu_cas('D', 5, 10); // D->C->B->A->G->F->E
echo keylist();
apcu_store('H', $value); // H->D->C->B->A->G->F
echo keylist();
apcu_store('I', str_repeat($value, 2)); // I->H->D->C->B->A
echo keylist();
apcu_store('J', $value); // J->I
echo keylist();

function keylist(): string
{
    $iter = new APCUIterator();
    $result = [];
    foreach ($iter as $v) {
        $result[] = $v['key'];
    }
    return implode(',', $result) . PHP_EOL;
}
?>
--EXPECT--
A
A,B
A,B
A,B,C
A,B,C
A,B,C
A,D,E
A,D,E
A,E,F
A,E,F
E,F
F
F,G,H
H,I

A,B,C,D,E,F,G
A,B,C,D,E,F,G
A,B,C,D,E,F,G
A,B,C,D,E,F,G
A,B,C,D,E,F,G
A,B,C,D,F,G,H
A,B,C,D,H,I
I,J
