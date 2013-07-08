--TEST--
APC: APCIterator formats 
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
apc.user_entries_hint=4096
--FILE--
<?php

$formats = array( 
                  APC_ITER_KEY,
                  APC_ITER_VALUE,
                  APC_ITER_NUM_HITS,
                  APC_ITER_MTIME,
                  APC_ITER_CTIME,
                  APC_ITER_DTIME,
                  APC_ITER_ATIME,
                  APC_ITER_REFCOUNT,
                  APC_ITER_MEM_SIZE,
                  APC_ITER_TTL,
                  APC_ITER_NONE,
                  APC_ITER_ALL,
                  APC_ITER_ALL & ~APC_ITER_TTL,
                  APC_ITER_KEY | APC_ITER_NUM_HITS | APC_ITER_MEM_SIZE,
                );

$it_array = array();

foreach ($formats as $idx => $format) {
	if (APCU_APC_FULL_BC) {
		$it_array[$idx] = new APCIterator('dummy', NULL, $format);
	} else {
		$it_array[$idx] = new APCIterator(NULL, $format);
	}
}

for($i = 0; $i < 11; $i++) {
  apc_store("key$i", "value$i");
}

foreach ($it_array as $idx => $it) {
  print_it($it, $idx);
}

function print_it($it, $idx) {
  echo "IT #$idx\n";
  echo "============================\n";
  foreach ($it as $key=>$value) {
    var_dump($key);
    var_dump($value);
  }
  echo "============================\n\n";
}

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
IT #0
============================
string(5) "key10"
array(1) {
  ["key"]=>
  string(5) "key10"
}
string(4) "key0"
array(1) {
  ["key"]=>
  string(4) "key0"
}
string(4) "key1"
array(1) {
  ["key"]=>
  string(4) "key1"
}
string(4) "key2"
array(1) {
  ["key"]=>
  string(4) "key2"
}
string(4) "key3"
array(1) {
  ["key"]=>
  string(4) "key3"
}
string(4) "key4"
array(1) {
  ["key"]=>
  string(4) "key4"
}
string(4) "key5"
array(1) {
  ["key"]=>
  string(4) "key5"
}
string(4) "key6"
array(1) {
  ["key"]=>
  string(4) "key6"
}
string(4) "key7"
array(1) {
  ["key"]=>
  string(4) "key7"
}
string(4) "key8"
array(1) {
  ["key"]=>
  string(4) "key8"
}
string(4) "key9"
array(1) {
  ["key"]=>
  string(4) "key9"
}
============================

IT #1
============================
string(5) "key10"
array(1) {
  ["value"]=>
  string(7) "value10"
}
string(4) "key0"
array(1) {
  ["value"]=>
  string(6) "value0"
}
string(4) "key1"
array(1) {
  ["value"]=>
  string(6) "value1"
}
string(4) "key2"
array(1) {
  ["value"]=>
  string(6) "value2"
}
string(4) "key3"
array(1) {
  ["value"]=>
  string(6) "value3"
}
string(4) "key4"
array(1) {
  ["value"]=>
  string(6) "value4"
}
string(4) "key5"
array(1) {
  ["value"]=>
  string(6) "value5"
}
string(4) "key6"
array(1) {
  ["value"]=>
  string(6) "value6"
}
string(4) "key7"
array(1) {
  ["value"]=>
  string(6) "value7"
}
string(4) "key8"
array(1) {
  ["value"]=>
  string(6) "value8"
}
string(4) "key9"
array(1) {
  ["value"]=>
  string(6) "value9"
}
============================

IT #2
============================
string(5) "key10"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key0"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key1"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key2"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key3"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key4"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key5"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key6"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key7"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key8"
array(1) {
  ["nhits"]=>
  int(0)
}
string(4) "key9"
array(1) {
  ["nhits"]=>
  int(0)
}
============================

IT #3
============================
string(5) "key10"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key0"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key1"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key2"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key3"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key4"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key5"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key6"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key7"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key8"
array(1) {
  ["mtime"]=>
  int(%d)
}
string(4) "key9"
array(1) {
  ["mtime"]=>
  int(%d)
}
============================

IT #4
============================
string(5) "key10"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key0"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key1"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key2"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key3"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key4"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key5"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key6"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key7"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key8"
array(1) {
  ["ctime"]=>
  int(%d)
}
string(4) "key9"
array(1) {
  ["ctime"]=>
  int(%d)
}
============================

IT #5
============================
string(5) "key10"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key0"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key1"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key2"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key3"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key4"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key5"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key6"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key7"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key8"
array(1) {
  ["dtime"]=>
  int(0)
}
string(4) "key9"
array(1) {
  ["dtime"]=>
  int(0)
}
============================

IT #6
============================
string(5) "key10"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key0"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key1"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key2"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key3"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key4"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key5"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key6"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key7"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key8"
array(1) {
  ["atime"]=>
  int(%d)
}
string(4) "key9"
array(1) {
  ["atime"]=>
  int(%d)
}
============================

IT #7
============================
string(5) "key10"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key0"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key1"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key2"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key3"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key4"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key5"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key6"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key7"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key8"
array(1) {
  ["ref_count"]=>
  int(0)
}
string(4) "key9"
array(1) {
  ["ref_count"]=>
  int(0)
}
============================

IT #8
============================
string(5) "key10"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key0"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key1"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key2"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key3"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key4"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key5"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key6"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key7"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key8"
array(1) {
  ["mem_size"]=>
  int(%d)
}
string(4) "key9"
array(1) {
  ["mem_size"]=>
  int(%d)
}
============================

IT #9
============================
string(5) "key10"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key0"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key1"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key2"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key3"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key4"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key5"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key6"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key7"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key8"
array(1) {
  ["ttl"]=>
  int(0)
}
string(4) "key9"
array(1) {
  ["ttl"]=>
  int(0)
}
============================

IT #10
============================
string(5) "key10"
array(0) {
}
string(4) "key0"
array(0) {
}
string(4) "key1"
array(0) {
}
string(4) "key2"
array(0) {
}
string(4) "key3"
array(0) {
}
string(4) "key4"
array(0) {
}
string(4) "key5"
array(0) {
}
string(4) "key6"
array(0) {
}
string(4) "key7"
array(0) {
}
string(4) "key8"
array(0) {
}
string(4) "key9"
array(0) {
}
============================

IT #11
============================
string(5) "key10"
array(10) {
  ["key"]=>
  string(5) "key10"
  ["value"]=>
  string(7) "value10"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key0"
array(10) {
  ["key"]=>
  string(4) "key0"
  ["value"]=>
  string(6) "value0"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key1"
array(10) {
  ["key"]=>
  string(4) "key1"
  ["value"]=>
  string(6) "value1"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key2"
array(10) {
  ["key"]=>
  string(4) "key2"
  ["value"]=>
  string(6) "value2"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key3"
array(10) {
  ["key"]=>
  string(4) "key3"
  ["value"]=>
  string(6) "value3"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key4"
array(10) {
  ["key"]=>
  string(4) "key4"
  ["value"]=>
  string(6) "value4"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key5"
array(10) {
  ["key"]=>
  string(4) "key5"
  ["value"]=>
  string(6) "value5"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key6"
array(10) {
  ["key"]=>
  string(4) "key6"
  ["value"]=>
  string(6) "value6"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key7"
array(10) {
  ["key"]=>
  string(4) "key7"
  ["value"]=>
  string(6) "value7"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key8"
array(10) {
  ["key"]=>
  string(4) "key8"
  ["value"]=>
  string(6) "value8"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
string(4) "key9"
array(10) {
  ["key"]=>
  string(4) "key9"
  ["value"]=>
  string(6) "value9"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
  ["ttl"]=>
  int(0)
}
============================

IT #12
============================
string(5) "key10"
array(9) {
  ["key"]=>
  string(5) "key10"
  ["value"]=>
  string(7) "value10"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key0"
array(9) {
  ["key"]=>
  string(4) "key0"
  ["value"]=>
  string(6) "value0"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key1"
array(9) {
  ["key"]=>
  string(4) "key1"
  ["value"]=>
  string(6) "value1"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key2"
array(9) {
  ["key"]=>
  string(4) "key2"
  ["value"]=>
  string(6) "value2"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key3"
array(9) {
  ["key"]=>
  string(4) "key3"
  ["value"]=>
  string(6) "value3"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key4"
array(9) {
  ["key"]=>
  string(4) "key4"
  ["value"]=>
  string(6) "value4"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key5"
array(9) {
  ["key"]=>
  string(4) "key5"
  ["value"]=>
  string(6) "value5"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key6"
array(9) {
  ["key"]=>
  string(4) "key6"
  ["value"]=>
  string(6) "value6"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key7"
array(9) {
  ["key"]=>
  string(4) "key7"
  ["value"]=>
  string(6) "value7"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key8"
array(9) {
  ["key"]=>
  string(4) "key8"
  ["value"]=>
  string(6) "value8"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key9"
array(9) {
  ["key"]=>
  string(4) "key9"
  ["value"]=>
  string(6) "value9"
  ["nhits"]=>
  int(0)
  ["mtime"]=>
  int(%d)
  ["ctime"]=>
  int(%d)
  ["dtime"]=>
  int(0)
  ["atime"]=>
  int(%d)
  ["ref_count"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
============================

IT #13
============================
string(5) "key10"
array(3) {
  ["key"]=>
  string(5) "key10"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key0"
array(3) {
  ["key"]=>
  string(4) "key0"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key1"
array(3) {
  ["key"]=>
  string(4) "key1"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key2"
array(3) {
  ["key"]=>
  string(4) "key2"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key3"
array(3) {
  ["key"]=>
  string(4) "key3"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key4"
array(3) {
  ["key"]=>
  string(4) "key4"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key5"
array(3) {
  ["key"]=>
  string(4) "key5"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key6"
array(3) {
  ["key"]=>
  string(4) "key6"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key7"
array(3) {
  ["key"]=>
  string(4) "key7"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key8"
array(3) {
  ["key"]=>
  string(4) "key8"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
string(4) "key9"
array(3) {
  ["key"]=>
  string(4) "key9"
  ["nhits"]=>
  int(0)
  ["mem_size"]=>
  int(%d)
}
============================

===DONE===
