--TEST--
Bug #62757 (php-fpm carshed when used apc_bin_dumpfile with apc.serializer)
--INI--
apc.enabled=1
apc.enable_cli=1
apc.stat=0
apc.cache_by_default=1
apc.serializer=php
report_memleaks=0
--FILE--
<?php
$filename = dirname(__FILE__) . '/bug62757_file.php';
$bin_filename = dirname(__FILE__)  . "/bug62757.bin";
$file_contents = '<?php
function test($arr=array()) {
    return $arr;
}

class ApiLib {
    private $arr = array( \'abcd\' => array() );
    protected $str = "constant string";
    public function test() {
         var_dump($this->str);
         return $this->arr;
    }
}
';
file_put_contents($filename, $file_contents);
apc_compile_file($filename);
apc_bin_dumpfile(array($filename), null, $bin_filename);
apc_clear_cache();
apc_bin_loadfile($bin_filename);
include $filename;

var_dump(test());
$lib = new ApiLib();
var_dump($lib->test());
echo "okey\n";
?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . '/bug62757_file.php');
unlink(dirname(__FILE__)  . "/bug62757.bin");
?>
--EXPECT--
array(0) {
}
string(15) "constant string"
array(1) {
  ["abcd"]=>
  array(0) {
  }
}
okey
