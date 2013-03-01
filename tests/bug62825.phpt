--TEST--
Bug #62825 (php carshed OR return PHP Fatal error when used apc_bin_dump after apc_store)
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
report_memleaks=0
--FILE--
<?php
apc_store('k1','testvalue');
apc_store('k2', array('foo' => array()));
apc_store('k3', array('foo' => new StdClass()));
$obj = new StdClass();
$obj->name = array('foo');
apc_store('k4', $obj);
apc_store('k5', NULL);
apc_store('k6', 0.5);
apc_store('k7', 123456);
$dump = apc_bin_dump(array(), NULL);
apc_clear_cache();
var_dump(apc_fetch('testkey'));
apc_bin_load($dump, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
foreach(range(1, 7) as $i) {
  var_dump(apc_fetch("k" . $i));
}
?>
--EXPECTF--
bool(false)
string(9) "testvalue"
array(1) {
  ["foo"]=>
  array(0) {
  }
}
array(1) {
  ["foo"]=>
  object(stdClass)#%d (0) {
  }
}
object(stdClass)#%d (1) {
  ["name"]=>
  array(1) {
    [0]=>
    string(3) "foo"
  }
}
NULL
float(0.5)
int(123456)
