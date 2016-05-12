--TEST--
Copy failure should not create entry
--FILE--
<?php
try {
	apcu_store('thing', function(){});
} catch(Exception $ex) {
}

var_dump(apcu_exists('thing'));
--EXPECT--
bool(false)
