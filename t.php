<?php
apc_store("test", "whatever");
var_dump(apc_fetch("test"));
?>
