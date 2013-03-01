--TEST--
APC: apc_store/fetch with objects (php pre-5.3)
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
    if(version_compare(zend_version(), '2.3.0') >= 0) {
		echo "skip\n";
	}
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

class foo { }
$foo = new foo;
var_dump($foo);
apc_store('foo',$foo);
unset($foo);
$bar = apc_fetch('foo');
var_dump($bar);
$bar->a = true;
var_dump($bar);

class bar extends foo
{
	public    $pub = 'bar';
	protected $pro = 'bar';
	private   $pri = 'bar'; // we don't see this, we'd need php 5.1 new serialization
	
	function __construct()
	{
		$this->bar = true;
	}
	
	function change()
	{
		$this->pri = 'mod';
	}
}

class baz extends bar
{
	private $pri = 'baz';

	function __construct()
	{
		parent::__construct();
		$this->baz = true;
	}
}

$baz = new baz;
var_dump($baz);
$baz->change();
var_dump($baz);
apc_store('baz', $baz);
unset($baz);
var_dump(apc_fetch('baz'));

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
object(foo)#%d (0) {
}
object(foo)#%d (0) {
}
object(foo)#%d (1) {
  ["a"]=>
  bool(true)
}
object(baz)#%d (6) {
  ["pri:private"]=>
  string(3) "baz"
  ["pub"]=>
  string(3) "bar"
  ["pro:protected"]=>
  string(3) "bar"
  ["pri:private"]=>
  string(3) "bar"
  ["bar"]=>
  bool(true)
  ["baz"]=>
  bool(true)
}
object(baz)#%d (6) {
  ["pri:private"]=>
  string(3) "baz"
  ["pub"]=>
  string(3) "bar"
  ["pro:protected"]=>
  string(3) "bar"
  ["pri:private"]=>
  string(3) "mod"
  ["bar"]=>
  bool(true)
  ["baz"]=>
  bool(true)
}
object(baz)#%d (6) {
  ["pri:private"]=>
  string(3) "baz"
  ["pub"]=>
  string(3) "bar"
  ["pro:protected"]=>
  string(3) "bar"
  ["pri:private"]=>
  string(3) "mod"
  ["bar"]=>
  bool(true)
  ["baz"]=>
  bool(true)
}
===DONE===
