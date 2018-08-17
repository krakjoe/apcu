<?php

/* based on sapi/cli/tests/php_cli_server.inc */
const PHP_CLI_SERVER_HOSTNAME = '127.0.0.1';
const PHP_CLI_SERVER_PORT = 8000 + (PHP_INT_SIZE * 100) + (PHP_MAJOR_VERSION * 10) + PHP_MINOR_VERSION;
const PHP_CLI_SERVER_ADDRESS = PHP_CLI_SERVER_HOSTNAME . ":" . PHP_CLI_SERVER_PORT;

class ServerTestGlobals
{
	const IS_WINDOWS = (PHP_OS & "\xDF\xDF\xDF") === 'WIN';

	public static $doc_root = __DIR__;
    public static $router = "index.php";
	public static $handles = [];
	public static $num_servers = 3;

	public static function router_full_path()
	{
		return self::$doc_root . DIRECTORY_SEPARATOR . self::$router;
	}

	public static function router_arg_path($no_router)
	{
		if ($no_router) {
			return '';
		}

		return self::IS_WINDOWS
			? self::$router
			: self::router_full_path();
	}
}

class PhpArgs
{
	const COMMAND_LINE_PARSE_PATTERN = '(
		.*?
		(?:
			-d\s*
			(?:
				(?P<qt1>[\'"]) (?P<op1>(?:(?!(?P=qt1)).)*) (?P=qt1)
			|	(?P<op2>\S+)
			)
		|	(?P<qt2>[\'"]) -d (?P<op3>(?:(?!(?P=qt2)).)*) (?P=qt2)
		)
	)Ax';

	private $settings = [];
	private $extensions = [];
	private $extension_dir;

	private static function get_default_extension_dir()
	{
		$path = ServerTestGlobals::$doc_root . '/..';

		if (!ServerTestGlobals::IS_WINDOWS) {
			return $path . '/modules';
		}

		if (PHP_INT_SIZE === 8) {
			$path .= '/x64';
		}

		$path .= ZEND_DEBUG_BUILD ? "/Debug" : "/Release";

		if (PHP_ZTS) {
			$path .= '_TS';
		}

		return $path;
	}

	public function __construct()
	{
		$this->extension_dir = self::get_default_extension_dir();
	}

	public function to_command_line()
	{
		$settings = ["extension_dir={$this->extension_dir}"];

		foreach ($this->extensions as $extension) {
			$settings[] = "extension={$extension}";
		}

		foreach ($this->settings as $name => $value) {
			$settings[] = "{$name}={$value}";
		}

		return '-d ' . implode(' -d ', array_map('escapeshellarg', $settings));
	}

	public function parse_settings_from_command_line_args($args)
	{
		for ($offset = 0; isset($args[$offset]); $offset += strlen($match[0])) {
			if (!preg_match(self::COMMAND_LINE_PARSE_PATTERN, $args, $match, 0, $offset)) {
				break;
			}

			$this->parse_setting($match['op3'] ?? $match['op2'] ?? $match['op1']);
		}

		return $this;
	}

	public function parse_settings($settings)
	{
		foreach ((array)$settings as $setting) {
			$this->parse_setting($setting);
		}

		return $this;
	}

	public function parse_setting($setting)
	{
		if (!preg_match('#^(?P<name>[^=]+)=(?P<value>.*)$#', $setting, $match)) {
			return $this;
		}

		switch ($match['name']) {
			case 'extension':
				$this->extensions[basename($match['value'])] = $match['value'];
				break;

			case 'extension_dir':
				$this->extension_dir = $match['value'];
				break;

			default:
				$this->settings[$match['name']] = $match['value'];
		}

		return $this;
	}

	public function add_extension($name)
	{
		$file = ServerTestGlobals::IS_WINDOWS
			? "php_{$name}.dll"
			: "{$name}.so";

		$this->extensions[$file] = $file;

		return $this;
	}
}

function server_start_one($host, $port, $php_opts = [], $no_router = false)
{
	$php_executable = getenv('TEST_PHP_EXECUTABLE');

	$php_args = (new PhpArgs)
		->parse_settings_from_command_line_args(getenv('TEST_PHP_ARGS'))
		->parse_settings($php_opts)
		->add_extension('apcu')
	;

	$cmd = sprintf(
		'%s -n %s -t %s -S %s %s',
		escapeshellarg($php_executable),
		$php_args->to_command_line(),
		escapeshellarg(ServerTestGlobals::$doc_root),
		escapeshellarg("{$host}:{$port}"),
		escapeshellarg(ServerTestGlobals::router_arg_path($no_router))
	);

	$cwd = ServerTestGlobals::$doc_root;
	$env = $opts = null;

	if (ServerTestGlobals::IS_WINDOWS) {
		$opts = ["bypass_shell" => true, "suppress_errors" => true];
	} else {
		$cmd = "exec {$cmd} 2>/dev/null";
	}

	$handle = proc_open($cmd, [STDIN, STDOUT, STDERR], $pipes, $cwd, $env, $opts);

	// note: even when server prints 'Listening on localhost:8964...Press Ctrl-C to quit.'
	//       it might not be listening yet...need to wait until fsockopen() call returns
	for ($i = 0; $i < 10 && !connection_test($host, $port); $i++) {
		usleep(100000);
	}

	return $handle;
}

function server_start($code = 'echo "Hello world";', $php_opts = [], $no_router = false)
{
	if ($code) {
		file_put_contents(ServerTestGlobals::router_full_path(), '<?php ' . $code . ' ?>');
	}

	for ($i = 0; $i < ServerTestGlobals::$num_servers; $i++) {
		ServerTestGlobals::$handles[] = server_start_one(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT + $i, $php_opts, $no_router);
	}

	register_shutdown_function(function() {
		foreach (ServerTestGlobals::$handles as $handle) {
			proc_terminate($handle);
		}
		@unlink(ServerTestGlobals::router_full_path());
	});
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
	$port = intval($port) ?: 80;

	$fp = @fsockopen($host, $port, $errno, $errstr, 10);
	if (!$fp) {
		return false;
	}

	$send = "GET / HTTP/1.1\nHost: {$host}\r\n\r\n";

	/* will not out here, just test if the connection has worked*/
	if (@fwrite($fp, $send)) {
		get_response($fp);
		fclose($fp);

		return true;
	}

	@fclose($fp);

	return false;
}

function run_test_simple($request_uri = null)
{
	$send = "GET /" . $request_uri . " HTTP/1.1\nHost: " . PHP_CLI_SERVER_HOSTNAME . "\r\n\r\n";

	for ($i = 0; $i < ServerTestGlobals::$num_servers; $i++) {
		run_test(PHP_CLI_SERVER_HOSTNAME, PHP_CLI_SERVER_PORT + $i, $send);
	}
}

function run_test($host, $port, $send)
{
	$fp = fsockopen($host, $port, $errno, $errstr, 10);
	if (!$fp) {
		die(sprintf("connect failed errno=%d errstr='%s'", $errno, $errstr));
	}

	if (fwrite($fp, $send)) {
		echo get_response($fp);
	}

	fclose($fp);
}
