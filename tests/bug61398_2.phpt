--TEST--
Bug #61398 APC fails to find class methods (based on ext/standard/tests/file/fopencookie.phpt)
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

$file = <<<FL
class userstream {
	public \$position = 0;
	public \$data = "If you can read this, it worked\n";
	
	function stream_open(\$path, \$mode, \$options, &\$opened_path)
	{
		return true;
	}
	
	function stream_read(\$count)
	{
		\$ret = substr(\$this->data, \$this->position, \$count);
		\$this->position += strlen(\$ret);
		return \$ret;
	}

	function stream_tell()
	{
		return \$this->position;
	}

	function stream_eof()
	{
		return \$this->position >= strlen(\$this->data);
	}

	function stream_seek(\$offset, \$whence)
	{
		switch(\$whence) {
			case SEEK_SET:
				if (\$offset < strlen(\$this->data) && \$offset >= 0) {
					\$this->position = \$offset;
					return true;
				} else {
					return false;
				}
				break;
			case SEEK_CUR:
				if (\$offset >= 0) {
					\$this->position += \$offset;
					return true;
				} else {
					return false;
				}
				break;
			case SEEK_END:
				if (strlen(\$this->data) + \$offset >= 0) {
					\$this->position = strlen(\$this->data) + \$offset;
					return true;
				} else {
					return false;
				}
				break;
			default:
				return false;
		}
	}
	function stream_stat() {
		return array('size' => strlen(\$this->data));
	}
}
stream_wrapper_register("cookietest", "userstream");
include("cookietest://foo");
FL;

server_start($file, $args);

for ($i = 0; $i < 10; $i++) {
	run_test_simple();
}
echo 'done';
?>
--EXPECT--
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
If you can read this, it worked
done
