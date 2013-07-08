--TEST--
APC: APCIterator chunk size
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
--FILE--
<?php

if (APCU_APC_FULL_BC) {
	$it = new APCIterator('user', NULL, APC_ITER_ALL, 10);
} else {
	$it = new APCIterator(NULL, APC_ITER_ALL, 10);
}
for($i = 0; $i < 41; $i++) {
  apc_store("key$i", "value$i");
}
foreach($it as $key=>$value) {
  $vals[$key] = $value['key'];
}
ksort($vals);
var_dump($vals);

?>
===DONE===
<?php exit(0); ?>
--EXPECT--
array(41) {
  ["key0"]=>
  string(4) "key0"
  ["key1"]=>
  string(4) "key1"
  ["key10"]=>
  string(5) "key10"
  ["key11"]=>
  string(5) "key11"
  ["key12"]=>
  string(5) "key12"
  ["key13"]=>
  string(5) "key13"
  ["key14"]=>
  string(5) "key14"
  ["key15"]=>
  string(5) "key15"
  ["key16"]=>
  string(5) "key16"
  ["key17"]=>
  string(5) "key17"
  ["key18"]=>
  string(5) "key18"
  ["key19"]=>
  string(5) "key19"
  ["key2"]=>
  string(4) "key2"
  ["key20"]=>
  string(5) "key20"
  ["key21"]=>
  string(5) "key21"
  ["key22"]=>
  string(5) "key22"
  ["key23"]=>
  string(5) "key23"
  ["key24"]=>
  string(5) "key24"
  ["key25"]=>
  string(5) "key25"
  ["key26"]=>
  string(5) "key26"
  ["key27"]=>
  string(5) "key27"
  ["key28"]=>
  string(5) "key28"
  ["key29"]=>
  string(5) "key29"
  ["key3"]=>
  string(4) "key3"
  ["key30"]=>
  string(5) "key30"
  ["key31"]=>
  string(5) "key31"
  ["key32"]=>
  string(5) "key32"
  ["key33"]=>
  string(5) "key33"
  ["key34"]=>
  string(5) "key34"
  ["key35"]=>
  string(5) "key35"
  ["key36"]=>
  string(5) "key36"
  ["key37"]=>
  string(5) "key37"
  ["key38"]=>
  string(5) "key38"
  ["key39"]=>
  string(5) "key39"
  ["key4"]=>
  string(4) "key4"
  ["key40"]=>
  string(5) "key40"
  ["key5"]=>
  string(4) "key5"
  ["key6"]=>
  string(4) "key6"
  ["key7"]=>
  string(4) "key7"
  ["key8"]=>
  string(4) "key8"
  ["key9"]=>
  string(4) "key9"
}
===DONE===
