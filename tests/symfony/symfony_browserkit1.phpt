--TEST--
Symfony BrowserKit ClientTest#testInsulatedRequests #1
--SKIPIF--
<?php
require_once(dirname(__FILE__) . '/../skipif.inc'); 
if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
	die('skip PHP 5.4+ only');
}
if(!function_exists('proc_open')) print 'skip'; ?>
--INI--
apc.enabled=1
apc.enable_cli=1
--FILE--
<?php

// must set TEST_PHP_EXECUTABLE env var to php.exe

// Crashes on Windows with APC
// 
// Get Exit Code
//  Linux: echo $?
//  Windows: echo %ErrorLevel%

require_once 'symfony_tests.inc';

use Symfony\Component\DomCrawler\Crawler;
use Symfony\Component\BrowserKit\Response;
use Symfony\Component\BrowserKit\Request;
use Symfony\Component\Process\PhpProcess;

// this doesn't crash, but crashes when same operation is done in TestClient#createCrawlerFromContent
//$uri = "http://www.example.com/foo/foobar";
//$crawler = new Crawler(null, $uri);

class TestClient {
    protected $nextResponse = null;
    protected $nextScript = null;
    protected $server;
    protected $request;
    protected $response;
    protected $crawler;
    protected $insulated;
    
    public function __construct(array $server = array(), History $history = null, CookieJar $cookieJar = null) {
        $this->setServerParameters($server);
        $this->insulated = false;
        $this->followRedirects = true;
    }

    public function followRedirects($followRedirect = true) {
        $this->followRedirects = (Boolean) $followRedirect;
    }

    public function insulate($insulated = true) {
        $this->insulated = (Boolean) $insulated;
    }

    public function setServerParameters(array $server) {
        $this->server = array_merge(array(
            'HTTP_HOST'       => 'localhost',
            'HTTP_USER_AGENT' => 'Symfony2 BrowserKit',
        ), $server);
    }

    public function getServerParameter($key, $default = '') {
        return (isset($this->server[$key])) ? $this->server[$key] : $default;
    }

    public function request($method, $uri, array $parameters = array(), array $files = array(), array $server = array(), $content = null, $changeHistory = true) {
        $uri = $this->getAbsoluteUri($uri);

        $server = array_merge($this->server, $server);
        $server['HTTP_HOST'] = parse_url($uri, PHP_URL_HOST);
        $server['HTTPS'] = 'https' == parse_url($uri, PHP_URL_SCHEME);

        $request = new Request($uri, $method, $parameters, $files, array(), $server, $content);

        $this->request = $this->filterRequest($request);

        if ($this->insulated) {
            $this->response = $this->doRequestInProcess($this->request);
        } else {
            $this->response = $this->doRequest($this->request);
        }

        $response = $this->filterResponse($this->response);

        $this->redirect = $response->getHeader('Location');

		return $this->crawler = $this->createCrawlerFromContent($request->getUri(), $response->getContent(), $response->getHeader('Content-Type'));
    }
	
	protected function doRequest($request)
    {
        if (null === $this->nextResponse) {
            return new Response();
        }

        $response = $this->nextResponse;
        $this->nextResponse = null;

        return $response;
    }

    protected function doRequestInProcess($request) {
        // We set the TMPDIR (for Macs) and TEMP (for Windows), because on these platforms the temp directory changes based on the user.
        $process = new PhpProcess($this->getScript($request), null, array('TMPDIR' => sys_get_temp_dir(), 'TEMP' => sys_get_temp_dir()));
		$process->run();
		
        if (!$process->isSuccessful() || !preg_match('/^O\:\d+\:/', $process->getOutput())) {
            throw new \RuntimeException('OUTPUT: '.$process->getOutput().' ERROR OUTPUT: '.$process->getErrorOutput());
        }
        return unserialize($process->getOutput());
    }

    protected function filterRequest(Request $request) {
        return $request;
    }

    protected function filterResponse($response) {
        return $response;
    }

    protected function createCrawlerFromContent($uri, $content, $type) {
		// interestingly, just doing a test of 'new Crawler...' by itself, doesn't crash
		$crawler = new Crawler(null, $uri);
		
		// test crashes here
        //$crawler->addContent($content, $type);

        return $crawler;
    }

    protected function getAbsoluteUri($uri) {
        // already absolute?
        if (0 === strpos($uri, 'http')) {
            return $uri;
        }

        if (!$this->history->isEmpty()) {
            $currentUri = $this->history->current()->getUri();
        } else {
            $currentUri = sprintf('http%s://%s/',
                isset($this->server['HTTPS']) ? 's' : '',
                isset($this->server['HTTP_HOST']) ? $this->server['HTTP_HOST'] : 'localhost'
            );
        }

        // anchor?
        if (!$uri || '#' == $uri[0]) {
            return preg_replace('/#.*?$/', '', $currentUri).$uri;
        }

        if ('/' !== $uri[0]) {
            $path = parse_url($currentUri, PHP_URL_PATH);

            if ('/' !== substr($path, -1)) {
                $path = substr($path, 0, strrpos($path, '/') + 1);
            }

            $uri = $path.$uri;
        }

        return preg_replace('#^(.*?//[^/]+)\/.*$#', '$1', $currentUri).$uri;
    }

    protected function requestFromRequest(Request $request, $changeHistory = true) {
        return $this->request($request->getMethod(), $request->getUri(), $request->getParameters(), $request->getFiles(), $request->getServer(), $request->getContent(), $changeHistory);
    }

    public function setNextResponse(Response $response) {
        $this->nextResponse = $response;
    }

    public function setNextScript($script) {
        $this->nextScript = $script;
    }

    protected function getScript($request) {
	    $r = new \ReflectionClass('Symfony\Component\BrowserKit\Response');
        $path = $r->getFileName();

        return <<<EOF
<?php

require_once('$path');

echo serialize($this->nextScript);
EOF;
    }
}

// doesn't always crash. running this test 2+ times in same php process does not seem to increase odds of crash (maybe 2nd request issue)
$client = new TestClient();
$client->insulate();
$client->setNextScript("new Symfony\Component\BrowserKit\Response('foobar')");
$client->request('GET', 'http://www.example.com/foo/foobar');

echo "== didn't crash ==".PHP_EOL;

?>
--EXPECT--
== didn't crash ==
