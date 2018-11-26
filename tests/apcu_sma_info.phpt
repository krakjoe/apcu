--TEST--
Basic apcu_sma_info() test
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_segments=1
--FILE--
<?php

apcu_store("key", "value");
var_dump(apcu_sma_info(true));
var_dump(apcu_sma_info());

?>
--EXPECTF--
array(3) {
  ["num_seg"]=>
  int(1)
  ["seg_size"]=>
  float(%s)
  ["avail_mem"]=>
  float(%s)
}
array(4) {
  ["num_seg"]=>
  int(1)
  ["seg_size"]=>
  float(%s)
  ["avail_mem"]=>
  float(%s)
  ["block_lists"]=>
  array(1) {
    [0]=>
    array(1) {
      [0]=>
      array(2) {
        ["size"]=>
        int(%d)
        ["offset"]=>
        int(%d)
      }
    }
  }
}
