<?php

$my_class = New my_class();
$my_i_class = New my_i_class();

echo "apc bindump 002 test\n";
echo "\n";
echo "global scope execution:            Success\n";
echo "\n";
echo "function execution:                ".my_function()."\n";
echo "\n";

echo "class static method:               ".my_class::my_static_method()."\n";
echo "class dynamic method:              ".$my_class->my_method()."\n";
echo "class static property:             ".my_class::$my_static_property."\n";
echo "class dynamic property:            ".$my_class->my_property."\n";
echo "class constant:                    ".my_class::my_constant."\n";
echo "\n";
echo "inherited class static method:     ".my_i_class::my_static_method()."\n";
echo "inherited class dynamic method:    ".$my_i_class->my_method()."\n";
echo "inherited class static property:   ".my_i_class::$my_static_property."\n";
echo "inherited class dynamic property:  ".$my_i_class->my_property."\n";
echo "inherited class constant:          ".my_i_class::my_constant."\n";
echo "\n";



function my_function() { return "Success"; }


class my_class {
  static $my_static_property = "Success";
  var $my_property = "Success";
  const my_constant = "Success";
  static function my_static_method() { return "Success"; }
  function my_method() { return "Success"; }
}

class my_i_class extends my_class {
  function dummy() { return 1; }
}

