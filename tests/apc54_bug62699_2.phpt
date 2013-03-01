--TEST--
APC: Bug #62699 Wrong behavior with traits (variant 2)
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
	static \$foo = 42;
}
class Foo {
    use A;
	static \$foo = 24;
    public function test() {
        var_dump(self::\$foo);
    }
}
\$foo = new Foo;
\$foo->test();
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
--EXPECTF--
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
<br />
<b>Fatal error</b>:  Foo and A define the same property ($foo) in the composition of Foo. However, the definition differs and is considered incompatible. Class was composed in <b>%sindex.php</b> on line <b>11</b><br />
done
