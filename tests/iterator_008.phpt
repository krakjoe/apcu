--TEST--
APC: APCUIterator should not create strings with shared memory c strings
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

class MyApc
{
    private $counterName = 'counter';

    public function setCounterName($value)
    {
        $this->counterName = $value;
    }

    public function getCounters($name)
    {
        $rex = '/^' . preg_quote($name) . '\./';
        $counters = array();

        foreach (new \APCUIterator($rex, APC_ITER_KEY|APC_ITER_VALUE) as $counter) {
            $counters[$counter['key']] = $counter['value'];
        }

        return $counters;
    }

    public function add($key, $data, $ttl = 0)
    {
        $ret =  apcu_store($key, $data, $ttl);

        if (true !== $ret) {
            throw new \UnexpectedValueException("apc_store call failed");
        }

        return $ret;
    }
}


$myapc = new MyApc();

$counterName = uniqid();
$myapc->setCounterName($counterName);
$myapc->add($counterName.'.test', 1);
$results = $myapc->getCounters($counterName);
var_export($results);
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

