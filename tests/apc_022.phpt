--TEST--
apcu_inc/dec() TTL parameter
--SKIPIF--
<?php
require_once(__DIR__ . '/skipif.inc');
if (!function_exists('apcu_inc_request_time')) die('skip APC debug build required');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.use_request_time=1
apc.ttl=0
--FILE--
<?php

echo "T+0:\n";

/* insert new entry with ttl 1 */
var_dump(apcu_inc("a", 1, $success, 1));

apcu_inc_request_time(1);
echo "T+1:\n";

/* old entry still exists -> ttl is not updated */
var_dump(apcu_inc("a", 1, $success, 2));

apcu_inc_request_time(2);
echo "T+3:\n";

/* old entry is expired -> insert new entry with ttl 2 */
var_dump(apcu_inc("a", 1, $success, 2));

apcu_inc_request_time(2);
echo "T+5:\n";

/* old entry still exists -> ttl is not updated */
var_dump(apcu_inc("a", 1, $success, 3));

apcu_inc_request_time(3);
echo "T+8:\n";

/* old entry is expired -> insert new entry with ttl 0 */
var_dump(apcu_inc("a"));

apcu_inc_request_time(4);
echo "T+12:\n";

/* old entry still exists -> ttl is not updated */
var_dump(apcu_inc("a", 1, $success, 1));

apcu_inc_request_time(3);
echo "T+15:\n";

/* old entry still exists -> ttl is not updated */
var_dump(apcu_inc("a", 1, $success, 1));

?>
--EXPECT--
T+0:
int(1)
T+1:
int(2)
T+3:
int(1)
T+5:
int(2)
T+8:
int(1)
T+12:
int(2)
T+15:
int(3)
