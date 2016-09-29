--TEST--
APC: GH Bug #185 memory corruption
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

        foreach (new \APCuIterator($rex) as $counter) {
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

var_dump($counterName = uniqid());
var_dump($myapc->setCounterName($counterName));
var_dump($myapc->add($counterName.'.test', 1));
var_dump($results = $myapc->getCounters($counterName));
?>
Done
--EXPECTF--
string(%d) "%s"
NULL
bool(true)
array(1) {
  ["%s.test"]=>
  int(1)
}
Done
