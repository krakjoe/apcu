--TEST--
Symfony ProcessTest#testCallbackIsExecutedForOutput
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

// must set TEST_PHP_EXECUTABLE env var to php.exe

// Crashes on Windows with APC (PHP_5_4)
// Get Exit Code
//  Linux: echo $?
//  Windows: echo %ErrorLevel%

require_once 'symfony_tests.inc';

use Symfony\Component\Process\Process;

// see PHPUnit_Framework_Assert - extended by PHPUnit_Framework_TestCase
function isTrue() {
	// moving PHPUnit_Framework_Constraint_IsTrue into a .inc file, or this .phpt file or
	// changing its name seems to avoid the crash
	//
	// gets here
	$t = new PHPUnit_Framework_Constraint_IsTrue;
	// doesn't get here
	return $t;
}

function assertTrue($val) {
	// crashes instantiating PHPUnit_Framework_Constraint_IsTrue
	isTrue();
	// crash is before getting here
	
	//assertThat($val, isTrue());
}

function assertThat($condition, $value) {
}
//////////


// see ProcessTest#testCallbackIsExecutedForOutput
$php = getenv('TEST_PHP_EXECUTABLE');
$p = new Process(sprintf($php.' -r %s', escapeshellarg('echo \'foo\';')));

$called = false;
$p->run(function ($type, $buffer) use (&$called) {
	$called = $buffer === 'foo';
});

assertTrue($called);


echo "== didn't crash ==".PHP_EOL;

?>
--EXPECT--
== didn't crash ==
