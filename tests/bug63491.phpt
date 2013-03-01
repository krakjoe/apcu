--TEST--
APC: Bug #63491 file_md5 value was wrong when use apc_bin_load function
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--FILE--
<?php
include "server_test.inc";

$script = dirname(__FILE__) . '/bug63491.test.php';
$bin = dirname(__FILE__) . '/bug63491.test.bin';

file_put_contents($script, "<?php\nclass Hello {}\n\$a = '42';\n");

$file = <<<FL
	if ('dump' == \$_GET['do']) {
		echo "dumping\n";
		include "$script";
		\$ret = apc_bin_dumpfile(array('$script'), NULL, '$bin');
		if (false === \$ret) {
			echo "fail\n";
		} else {
			echo "all good \n";
		}
	} else if ('load' == \$_GET['do']) {
		echo "loading\n";
		\$ret = apc_bin_loadfile('$bin');
		if (true !== \$ret) {
			echo "fail\n";
		} else {
			echo "all good \n";
		}
	}
	\$aci = apc_cache_info();
	foreach (\$aci['cache_list'] as \$item) {
		if (\$item['filename'] == '$script') {
			echo "md5 is {\$item['md5']}\n";
			break;
		}
	}
FL;

$args = array(
'apc.enabled=1',
'apc.enable_cli=1',
'apc.file_md5=1',
'apc.stat=0',
);

server_start($file, $args);
for ($u = 0; $u < 10; $u++) {
	/* dump the test file on one instance */
	$send = "GET /?do=dump HTTP/1.1\n" .
			"Host: " . PHP_CLI_SERVER_HOSTNAME . "\n" .
			"\r\n\r\n";
	run_test(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT, $send);

	/* and load it on another */
	$send = "GET /?do=load HTTP/1.1\n" .
			"Host: " . PHP_CLI_SERVER_HOSTNAME . "\n" .
			"\r\n\r\n";
	run_test(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT+1, $send);
}
echo 'done';

--CLEAN--
<?php
	$script = dirname(__FILE__) . '/bug63491.test.php';
	$bin = dirname(__FILE__) . '/bug63491.test.bin';
	unlink($script);
	unlink($bin);
--EXPECT--
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
dumping
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
loading
all good 
md5 is f0b9d73bccf99c764cf793773d4d13d1
done
