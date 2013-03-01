<?php
namespace Foo\Bar;

function sort() {
	$a = array(3,2,1,4);
	\sort($a);  // global scoping
	var_dump($a);
	return "IT WORKS";
}
class Baz {
	public $i = 1;
	protected $f = 3.14;
	private $s = "hello world";

	public function foo() {
		sort();
	}
}
