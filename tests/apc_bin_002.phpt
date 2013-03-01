--TEST--
APC: bindump file cache part 1
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.stat=0
apc.cache_by_default=1
apc.filters=
report_memleaks = Off
--FILE--
<?php

define('filename',dirname(__FILE__).'/apc_bin_002.inc');
define('filename1',dirname(__FILE__).'/apc_bin_002-1.inc');
define('filename2',dirname(__FILE__).'/apc_bin_002-2.inc');

copy(filename1, filename);
apc_compile_file(filename);
$data = apc_bin_dump(NULL, NULL);

apc_clear_cache();

copy(filename2, filename);
apc_bin_load($data, APC_BIN_VERIFY_MD5 | APC_BIN_VERIFY_CRC32);
include(filename);

unlink(filename);

?>
===DONE===
<?  php exit(0); ?>
--EXPECTF--
apc bindump 002 test

global scope execution:            Success

function execution:                Success

class static method:               Success
class dynamic method:              Success
class static property:             Success
class dynamic property:            Success
class constant:                    Success

inherited class static method:     Success
inherited class dynamic method:    Success
inherited class static property:   Success
inherited class dynamic property:  Success
inherited class constant:          Success

===DONE===
