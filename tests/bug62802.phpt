--TEST--
Bug #62802 (Crash when use apc_bin_dump/load)
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/skipif.inc');
if (version_compare(PHP_VERSION, "5.4.0") < 0) {
    die('skip for PHP < 5.4');
}
--INI--
apc.enabled=1
apc.enable_cli=1
apc.stat=0
apc.cache_by_default=1
report_memleaks=0
--FILE--
<?php
$filename = dirname(__FILE__) . '/bug62802_file.php';
$file_contents = '<?php
$a = "uuu";
';
file_put_contents($filename, $file_contents);
apc_compile_file($filename);
$data = apc_bin_dump(null, null);
apc_clear_cache();
apc_bin_load($data, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
include $filename;

var_dump($a);
?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . '/bug62802_file.php');
?>
--EXPECT--
string(3) "uuu"
