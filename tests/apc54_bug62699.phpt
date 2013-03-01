--TEST--
APC: Bug #62699 Wrong behavior with traits
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

trait A {
    public function test() {
        var_dump(__TRAIT__);
    }
	public function tast() {
		var_dump(__TRAIT__);
	}
}
class Foo {
    use A {
        A::test as test2;
        A::tast as public;
    }
    public function test() {
        var_dump(__CLASS__);
    }
}
\$foo = new Foo;
\$foo->test();
\$foo->test2();
\$foo->tast();
FL;

$args = array(
	'apc.enabled=1',
	'apc.cache_by_default=1',
	'apc.enable_cli=1',
	'apc.shm_segments=1',
	'apc.optimization=0',
	'apc.shm_size=384M',
	'apc.ttl=0',
	'apc.user_ttl=0',
	'apc.num_files_hint=4096',
	'apc.canonicalize=0',
	'apc.write_lock=1',
);

server_start($file, $args);

for ($i = 0; $i < 10; $i++) {
	run_test_simple();
}
echo 'done';
--EXPECT--
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
string(3) "Foo"
string(1) "A"
string(1) "A"
done
