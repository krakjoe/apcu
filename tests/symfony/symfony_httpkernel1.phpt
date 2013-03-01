--TEST--
Symfony HttpKernel ClientTest#testGetScript
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/../skipif.inc'); 
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

require_once 'symfony_tests.inc';

use Symfony\Component\HttpKernel\Client;
use Symfony\Component\HttpKernel\HttpKernel;
use Symfony\Component\HttpFoundation\Request;
use Symfony\Component\HttpFoundation\Response;
use Symfony\Component\HttpFoundation\Cookie;
use Symfony\Component\HttpFoundation\File\UploadedFile;
use Symfony\Component\HttpKernel\Tests\Fixtures\TestClient;
use Symfony\Component\HttpKernel\Tests\TestHttpKernel;

$client = new TestClient(new TestHttpKernel());
$client->insulate();
$client->request('GET', '/');

echo "== didn't crash ==".PHP_EOL;

?>
--EXPECT--
== didn't crash ==
