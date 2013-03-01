--TEST--
Trying to exclude trait method multiple times (origin Zend/tests/traits/error_010.phpt)
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
trait foo {
	public function test() { return 3; }
}
trait c {
	public function test() { return 2; }
}

trait b {
	public function test() { return 1; }
}

class bar {
	use foo, c { c::test insteadof foo; }
	use foo, c { c::test insteadof foo; }
}

\$x = new bar;
var_dump(\$x->test());
FL;

$args = array(
	'apc.enabled=1',
	'apc.cache_by_default=1',
	'apc.enable_cli=1',
	'apc.shm_segments=1',
	'apc.optimization=0',
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
--EXPECTF--
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
<br />
<b>Fatal error</b>:  Failed to evaluate a trait precedence (test). Method of trait foo was defined to be excluded multiple times in <b>%sindex.php</b> on line <b>15</b><br />
done
