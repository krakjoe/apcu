--TEST--
APC: apcu_store/fetch reference test
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (PHP_VERSION_ID >= 70300) die('skip Only for PHP < 7.3');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=php
report_memleaks=0
--FILE--
<?php

$a = 'a';
$b = array($a);
$c = array('c');
$b[] = &$c;
$b[] = &$c;
$d = 'd';
$b[] = &$d;
$b[] = &$d;
$b[] = &$d;
$e = 'e';
$b[] = $e;
$b[] = $e;
$f = array('f');
$f[] = &$f;
$b[] = &$f;
apcu_store('test', $b);
$x = apcu_fetch('test');
debug_zval_dump($x);

?>
===DONE===
--EXPECT--
array(9) refcount(2){
  [0]=>
  string(1) "a" refcount(1)
  [1]=>
  &array(1) refcount(1){
    [0]=>
    string(1) "c" refcount(1)
  }
  [2]=>
  &array(1) refcount(1){
    [0]=>
    string(1) "c" refcount(1)
  }
  [3]=>
  &string(1) "d" refcount(1)
  [4]=>
  &string(1) "d" refcount(1)
  [5]=>
  &string(1) "d" refcount(1)
  [6]=>
  string(1) "e" refcount(1)
  [7]=>
  string(1) "e" refcount(1)
  [8]=>
  &array(2) refcount(1){
    [0]=>
    string(1) "f" refcount(1)
    [1]=>
    &array(2) refcount(1){
      [0]=>
      string(1) "f" refcount(1)
      [1]=>
      *RECURSION*
    }
  }
}
===DONE===
