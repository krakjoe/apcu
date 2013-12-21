--TEST--
APC: Bug #59938 APCIterator fails with large user cache
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
//to fill apc cache (~200MB):
for(\$i=0;\$i<50000;\$i++) {
    \$value = str_repeat(md5(microtime()), 100);
    apc_store('test-niko-asdfasdfasdfkjasdflkasjdfasf'.\$i, \$value);
}

//then later (usually after a few minutes) this won't work correctly:
if (APCU_APC_FULL_BC) {
	\$it = new ApcIterator('user', '#^test-niko-asdfasdfasdfkjasdflkasjdfasf#');
} else {
	\$it = new ApcIterator('#^test-niko-asdfasdfasdfkjasdflkasjdfasf#');
}
var_dump(\$it->getTotalCount()); //returns 50000
var_dump(\$it->current()); //returns false on error
FL;

$args = array(
	'apc.enabled=1',
	'apc.enable_cli=1',
	'apc.shm_size=256M',
);

server_start($file, $args);

for ($i = 0; $i < 3; $i++) {
	run_test_simple();
}
echo 'done';

--EXPECTF--
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
int(50000)
array(11) {
  ["type"]=>
  string(4) "user"
  ["key"]=>
  string(42) "test-niko-asdfasdfasdfkjasdflkasjdfasf%d"
  ["value"]=>
  string(%d) "%s"
  ["num_hits"]=>
  int(0)
  ["modified_time"]=>
  int(%d)
  ["creation_time"]=>
  int(%d)
  ["deletion_time"]=>
  int(0)
  ["access_time"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
done
