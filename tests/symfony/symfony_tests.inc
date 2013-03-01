<?php

// renaming classes or moving classes into a new file (like to put several classes in a single .INC file)
// seems to cause the bug to not occur any more

// so, many of the symfony class files need to be included as is
// many functions aren't needed but have been left in case other symfony phpunit tests need to be ported


// see vendor/composer/autoload_namespaces.php
// changed 'Symfony' key to point to '.' so that code that does "use Symfony\" will load classes
// from ./Symfony

require_once 'PHPUnit/Autoload.php';
require_once 'vendor/autoload.php';

?>