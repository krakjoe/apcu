--TEST--
APC: Bug #52144 (Error: Base lambda function for closure not found)
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--FILE--
<?php
include "server_test.inc";
$num_servers = 1;

$lambdas = dirname(__FILE__) . '/lambdas.php';

$file = <<<PHP
if (empty(\$_GET['tested'])) {
   exit();
}

\$script = "$lambdas";
if (!file_exists(\$script)) {
    file_put_contents(\$script, '<?php 
        if (!function_exists("dummy")) {
            function dummy() {
                var_dump(__FUNCTION__);
            }
        }

        \$func1 = function() {
            var_dump(__FUNCTION__ . "#1");
        };

        \$func2 = function() {
            var_dump(__FUNCTION__ . "#2");
        };'
    ); 

    apc_clear_cache();
    ini_set("apc.cache_by_default", 0);
    include(\$script);
    ini_set("apc.cache_by_default", 1);
    include(\$script);
    var_dump("cached");
} else {
    include(\$script);
    dummy();
    \$func1();
    \$func2();
    unlink(\$script);
}
PHP;

$args = array(
	'apc.enabled=1',
	'apc.enable_cli=1',
    'apc.file_update_protection=0',
);

server_start($file, $args);

for ($i = 0; $i < 10; $i++) {
    echo "-----------------------------\n";
	run_test_simple("?tested=true");
}
--CLEAN--
<?php
unlink(dirname(__FILE__) . '/lambdas.php');
?>
--EXPECT--
-----------------------------
string(6) "cached"
-----------------------------
string(5) "dummy"
string(11) "{closure}#1"
string(11) "{closure}#2"
-----------------------------
string(6) "cached"
-----------------------------
string(5) "dummy"
string(11) "{closure}#1"
string(11) "{closure}#2"
-----------------------------
string(6) "cached"
-----------------------------
string(5) "dummy"
string(11) "{closure}#1"
string(11) "{closure}#2"
-----------------------------
string(6) "cached"
-----------------------------
string(5) "dummy"
string(11) "{closure}#1"
string(11) "{closure}#2"
-----------------------------
string(6) "cached"
-----------------------------
string(5) "dummy"
string(11) "{closure}#1"
string(11) "{closure}#2"
