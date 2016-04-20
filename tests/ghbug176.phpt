--TEST--
APC: GH Bug #176 preload_path segfaults with bad data
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
	if(PHP_ZTS === 1) {
		die('skip PHP non-ZTS only');
	}
--FILE--
<?php
include "server_test.inc";

$file = <<<FL
\$key = 'abc';
\$b = apcu_exists(\$key);
var_dump(\$b);
if (\$b) {
	\$\$key = apcu_fetch(\$key);
	var_dump(\$\$key);
}
FL;

$args = array(
	'apc.enabled=1',
	'apc.enable_cli=1',
	'apc.preload_path=' . dirname(__FILE__) . '/bad',
);

server_start($file, $args);

for ($i = 0; $i < 10; $i++) {
	run_test_simple();
}
echo 'done';

--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
done
