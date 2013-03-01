--TEST--
APC: apc_delete_file test
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
apc.stat=On
report_memleaks=0
--FILE--
<?php

$files = array( 'apc_009.php',
                'apc_009-1.php',
                'apc_009-2.php',
                'nofile.php',
              );

$olddir = getcwd();
chdir(__DIR__);

file_put_contents(__DIR__ . '/apc_009-1.php', '<?php echo "test file";');
file_put_contents(__DIR__ . '/apc_009-2.php', '<?php syntaxerrorhere!');

apc_compile_file($files[0]);
check_file($files[0]);
apc_delete_file($files[0]);
check_file($files[0]);

apc_compile_file($files[0]);
apc_delete_file(array($files[0]));
check_file($files[0]);

apc_compile_file($files[0]);
$it = new APCIterator('file');
apc_delete_file($it);
check_file($files[0]);

var_dump(apc_compile_file(array($files[0], $files[1])));
check_file(array($files[0], $files[1]));

var_dump(apc_compile_file($files));
check_file($files);

function check_file($files) {

  if (!is_array($files)) {
    $files = array($files);
  }

  $info = apc_cache_info('file');

  foreach ($files as $file) {
    $match = 0;
    foreach($info['cache_list'] as $cached_file) {
      if (stristr($cached_file['filename'], $file)) $match = 1;
    }
    if ($match) {
      echo "$file Found File\n";
    } else {
      echo "$file Not Found\n";
    }
  }
}
chdir($olddir);
?>
===DONE===
<?php exit(0); ?>
--CLEAN--
<?php
unlink('apc_009-1.php');
unlink('apc_009-2.php');
?>
--EXPECTF--
apc_009.php Found File
apc_009.php Not Found
apc_009.php Not Found
apc_009.php Not Found
array(0) {
}
apc_009.php Found File
apc_009-1.php Found File

Parse error: syntax error, unexpected '!' in %s/apc_009-2.php on line 1

Warning: apc_compile_file(): Error compiling apc_009-2.php in apc_compile_file. in %s/apc_009.php on line 32

Warning: apc_compile_file(): Error compiling nofile.php in apc_compile_file. in %s/apc_009.php on line 32
array(2) {
  ["apc_009-2.php"]=>
  int(-1)
  ["nofile.php"]=>
  int(-1)
}
apc_009.php Found File
apc_009-1.php Found File
apc_009-2.php Not Found
nofile.php Not Found
===DONE===
