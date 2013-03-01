--TEST--
Bug #61398 APC fails to find class methods (based on bug #38779)
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--FILE--
<?php
include "server_test.inc";

$args = array(
	'apc.enabled=1',
	'apc.cache_by_default=1',
	'apc.enable_cli=1',
);

$data = '<' . "?php \n\"\";ll l\n ?" . '>';
$file = <<<FL
class Loader {
	private \$position;
	private \$data;
	public function stream_open(\$path, \$mode, \$options, &\$opened_path)  {
		\$this->data = '$data';
		\$this->position = 0;
		return true;
	}
	function stream_read(\$count) {
		\$ret = substr(\$this->data, \$this->position, \$count);
		\$this->position += strlen(\$ret);
		return \$ret;
	}
	function stream_eof() {
		return \$this->position >= strlen(\$this->data);
	}
	function stream_stat() {
		return array('size' => strlen(\$this->data));
	}
}
stream_wrapper_register('Loader', 'Loader');
require 'Loader://qqq.php';
FL;

server_start($file, $args);

for ($i = 0; $i < 10; $i++) {
	run_test_simple();
}
echo 'done';
?>
--EXPECTF--	
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
<br />
<b>Parse error</b>:  syntax error, unexpected 'l' (T_STRING) in <b>Loader://qqq.php</b> on line <b>2</b><br />
done
