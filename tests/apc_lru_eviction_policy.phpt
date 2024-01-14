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
--FILE--
<?php
$cache_info = apcu_cache_info();
if ($cache_info['eviction_policy'] !== 'lru') {
    var_dump(true);
    exit;
}

$value = str_repeat('X', 300000); // 300KB

$expect = <<<LOG
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

LOG;

$keylist = '';
apcu_store('A', $value); // A
$keylist .= keylist();
apcu_add('B', $value); // B->A
$keylist .= keylist();
apcu_add('A', $value); // B->A (no effect)
$keylist .= keylist();
apcu_store('C', $value); // C->B->A
$keylist .= keylist();
apcu_fetch('A'); // A->C->B
$keylist .= keylist();
apcu_exists('B'); // A->C->B (no effect)
$keylist .= keylist();
apcu_store(['D' => $value, 'E' => $value]); // E->D->A
$keylist .= keylist();
apcu_entry('A', function ($key) use ($value) { return $value; }); // A->E->D
$keylist .= keylist();
apcu_entry('F', function ($key) use ($value) { return $value; }); // F->A->E
$keylist .= keylist();
apcu_fetch(['F', 'A']); // A->F->E
$keylist .= keylist();
apcu_delete('A'); // F->E
$keylist .= keylist();
apcu_delete('E'); // F
$keylist .= keylist();
apcu_store(['G' => $value, 'H' => $value]); // H->G->F
$keylist .= keylist();
apcu_store('I', str_repeat($value, 2)); // I->H
$keylist .= keylist();
apcu_clear_cache();
$keylist .= keylist();
apcu_store([
    'A' => 1,
    'B' => 2,
    'C' => 3,
    'D' => 4,
    'E' => $value,
    'F' => $value,
    'G' => $value,
]); // G->F->E->F->C->B->A
$keylist .= keylist();
apcu_inc('A'); // A->G->F->E->F->C->B
$keylist .= keylist();
apcu_dec('B'); // B->A->G->F->E->F->C
$keylist .= keylist();
apcu_cas('C', 3, 5); // C->B->A->G->F->E->F
$keylist .= keylist();
apcu_cas('D', 5, 10); // D->C->B->A->G->F->E
$keylist .= keylist();
apcu_store('H', $value); // H->D->C->B->A->G->F
$keylist .= keylist();
apcu_store('I', str_repeat($value, 2)); // I->H->D->C->B->A
$keylist .= keylist();
apcu_store('J', $value); // J->I
$keylist .= keylist();

var_dump($keylist === $expect);

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
bool(true)
