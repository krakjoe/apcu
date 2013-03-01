--TEST--
APC: Bug #63669 Cannot declare self-referencing constant
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--FILE--
<?php
include "server_test.inc";

$file = <<<FL
class foo {
	const AAA = 'x';
	const BBB = 'a';
	const CCC = 'a';
	const DDD = self::AAA;

	private static \$foo = array(
		self::BBB	=> 'a',
		self::CCC	=> 'b',
		self::DDD	=>  self::AAA
	);
	
	public static function test() {
		var_dump(self::\$foo);
	}
}

foo::test();
FL;

$args = array(
	'apc.enabled=1',
	'apc.enable_cli=1',
	'apc.shm_size=256M',
);

server_start($file, $args);

for ($i = 0; $i < 10; $i++) {
	run_test_simple();
}
echo 'done';

--EXPECTF--
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
array(2) {
  ["a"]=>
  string(1) "b"
  ["x"]=>
  string(1) "x"
}
done
