--TEST--
APC: apc_store/fetch reference test
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
apc.serializer=default
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
apc_store('test', $b);
$x = apc_fetch('test');
debug_zval_dump($x);

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
array(9) refcount(2){
  [0]=>
  string(1) "a" refcount(1)
  [1]=>
  &array(1) refcount(2){
    [0]=>
    string(1) "c" refcount(1)
  }
  [2]=>
  &array(1) refcount(2){
    [0]=>
    string(1) "c" refcount(1)
  }
  [3]=>
  &string(1) "d" refcount(3)
  [4]=>
  &string(1) "d" refcount(3)
  [5]=>
  &string(1) "d" refcount(3)
  [6]=>
  string(1) "e" refcount(2)
  [7]=>
  string(1) "e" refcount(2)
  [8]=>
  &array(2) refcount(2){
    [0]=>
    string(1) "f" refcount(1)
    [1]=>
    &array(2) refcount(2){
      [0]=>
      string(1) "f" refcount(1)
      [1]=>
      *RECURSION*
    }
  }
}
===DONE===
