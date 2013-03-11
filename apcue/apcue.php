<?php
$br = (php_sapi_name() == "cli")? "":"<br>";

if(!extension_loaded('apcue')) {
	dl('apcue.' . PHP_SHLIB_SUFFIX);
}
$module = 'apcue';
$functions = get_extension_funcs($module);
echo "Functions available in the test extension:$br\n";
foreach($functions as $func) {
    echo $func."$br\n";
}
echo "$br\n";
while ($I++<100000) {
	$data[]=rand($I, 100000) * time();
}

var_dump(apcue_set("data", $data));
var_dump(apcue_get("data"));
var_dump(apc_sma_info());
?>
