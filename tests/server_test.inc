<?php
/* based on sapi/cli/tests/php_cli_server.inc */
define ("PHP_CLI_SERVER_HOSTNAME", "127.0.0.1");
define ("PHP_CLI_SERVER_PORT", 8000+PHP_INT_SIZE);
define ("PHP_CLI_SERVER_ADDRESS", PHP_CLI_SERVER_HOSTNAME.":".PHP_CLI_SERVER_PORT);

/* XXX incapsulate all this globals into a class when have a favourable minute */
$doc_root = __DIR__;
$router = "index.php";
$handles = array();
$ports = array();
$num_servers = 3;

function server_start_one($host, $port, $code = 'echo "Hello world";', $php_opts = array(), $no_router = FALSE)
{
	global $doc_root, $router, $handles, $ports;

	$php_executable = getenv('TEST_PHP_EXECUTABLE');

	$descriptorspec = array(
		0 => STDIN,
		1 => STDOUT,
		2 => STDERR,
	);

	$php_args = getenv('TEST_PHP_ARGS');
	if (empty($php_args)) {
		$ext = (substr(PHP_OS, 0, 3) == 'WIN') ? 'php_apcu.dll' : 'apcu.so';
		$php_args = "-d extension_dir=$doc_root/../modules -d extension=$ext";
	}
	if ($php_opts) {
		$php_args = "$php_args -d " . implode(' -d ', $php_opts);;
	}

	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$cmd = "{$php_executable} -n $php_args -t {$doc_root} -S $host:$port";
		if (!$no_router) {
			$cmd .= " {$router}";
		}

		$handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, $doc_root, NULL, array("bypass_shell" => true,  "suppress_errors" => true));
	} else {
		$cmd = "exec {$php_executable} -n $php_args -t {$doc_root} -S $host:$port";
		if (!$no_router) {
			$cmd .= " {$doc_root}/{$router}";
		}
		$cmd .= " 2>/dev/null";

		$handle = proc_open($cmd, $descriptorspec, $pipes, $doc_root);
	}
	
	// note: even when server prints 'Listening on localhost:8964...Press Ctrl-C to quit.'
	//       it might not be listening yet...need to wait until fsockopen() call returns
    $i = 0;
    while (($i++ < 10) && !connection_test($host, $port)) {
        usleep(100000);
    }

	return $handle;
}

function server_start($code = 'echo "Hello world";', $php_opts = array(), $no_router = FALSE)
{
	global $doc_root, $router, $handles, $ports, $num_servers;


	if ($code) {
		file_put_contents($doc_root . '/' . $router, '<?php ' . $code . ' ?>');
	}

	for ($i = 0; $i < $num_servers; $i++) {
		$h = server_start_one(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT+$i, $code, $php_opts, $no_router);
		$handles[] = $h;
	}	

	register_shutdown_function(
		function($handles) use($router) {
			foreach ($handles as $handle) {
				proc_terminate($handle);
			}
			@unlink(__DIR__ . "/{$router}");
		},
			$handles
		);
	// don't bother sleeping, server is already up
	// server can take a variable amount of time to be up, so just sleeping a guessed amount of time
	// does not work. this is why tests sometimes pass and sometimes fail. to get a reliable pass
	// sleeping doesn't work.
}

function get_response($fp, $data_only = true)
{
	$s = '';

	while (!feof($fp)) {
		$s .= fgets($fp);
	}

	if ($data_only) {
		$parts = explode("\r\n\r\n", $s);
		$s = $parts[1];
	}

	return $s;
}


function connection_test($host, $port)
{
	$port = intval($port)?:80;

	$fp = @fsockopen($host, $port, $errno, $errstr, 2);
	if (!$fp) {
		return false;
	}

	$send = "GET / HTTP/1.1\nHost: {$host}\r\n\r\n";

	/* will not out here, just test if the connection has worked*/
	if(@fwrite($fp, $send)) {
		get_response($fp);
		fclose($fp);

		return true;
	}

	@fclose($fp);

	return false;
}

function run_test_simple($request_uri = NULL)
{
    global $num_servers;
	$send = "GET /" . $request_uri ." HTTP/1.1\nHost: " . PHP_CLI_SERVER_HOSTNAME . "\r\n\r\n";

	for ($i = 0; $i < $num_servers; $i++) {
		run_test(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT+$i, $send);
	}
}

function run_test($host, $port, $send)
{
	$fp = fsockopen($host, $port, $errno, $errstr, 3);
	if (!$fp) {
	  die(sprintf("connect failed errno=%d errstr='%s'", $errno, $errstr));
	}

	if(fwrite($fp, $send)) {
		echo get_response($fp);
	}

	fclose($fp);
}
