--TEST--
APC: Bug #63224 error in __sleep whit reference to other classes
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc'); 
	if (!extension_loaded("session")) die("skip");
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--FILE--
<?php
include "server_test.inc";

$file = <<<FL
session_start();

class A{
	public \$b;
	
	public function __sleep(){
		\$this->b->f();
		return array('b');
	}
}


class B{
	const A_CONSTANT = 1;
	public \$var;

	public function f(){
		\$this->var = self::A_CONSTANT;
	}
}


if(isset(\$_SESSION['lalala'])){
	echo "<pre>";
	\$a = \$_SESSION['lalala'];
	print_r(\$a);
} else {
	echo "no session yet, first run\n";
}

//	another file
//	class A and B use autoload
\$b = new B();
\$a = new A();
\$a->b = \$b;

\$_SESSION['lalala'] = \$a;
session_write_close();
FL;

$args = array(
	'apc.enabled=1',
	'apc.cache_by_default=1',
	'apc.enable_cli=1',
);

server_start($file, $args);

$sid = md5(uniqid("call me maybe", true));
for ($i = 0; $i < 10; $i++) {
	$send = "GET / HTTP/1.1\n" .
			"Host: " . PHP_CLI_SERVER_HOSTNAME . "\n" .
			"Cookie: PHPSESSID=$sid;" .
			"\r\n\r\n";
	for ($j = 0; $j < $num_servers; $j++) {
		run_test(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT+$j, $send);
	}
}
echo 'done';
--EXPECT--
no session yet, first run
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
<pre>A Object
(
    [b] => B Object
        (
            [var] => 1
        )

)
done
