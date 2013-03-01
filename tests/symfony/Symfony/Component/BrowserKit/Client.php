<?php

namespace Symfony\Component\BrowserKit;

use Symfony\Component\DomCrawler\Crawler;
use Symfony\Component\DomCrawler\Link;
use Symfony\Component\DomCrawler\Form;
use Symfony\Component\Process\PhpProcess;

abstract class Client
{
    protected $history;
    protected $cookieJar;
    protected $server;
    protected $request;
    protected $response;
    protected $crawler;
    protected $insulated;
    protected $redirect;
    protected $followRedirects;

    public function __construct(array $server = array(), History $history = null, CookieJar $cookieJar = null)
    {
        $this->setServerParameters($server);
        $this->history = null === $history ? new History() : $history;
        $this->cookieJar = null === $cookieJar ? new CookieJar() : $cookieJar;
        $this->insulated = false;
        $this->followRedirects = true;
    }

    public function followRedirects($followRedirect = true)
    {
        $this->followRedirects = (Boolean) $followRedirect;
    }

    public function insulate($insulated = true)
    {
        if ($insulated && !class_exists('Symfony\\Component\\Process\\Process')) {
            // @codeCoverageIgnoreStart
            throw new \RuntimeException('Unable to isolate requests as the Symfony Process Component is not installed.');
            // @codeCoverageIgnoreEnd
        }

        $this->insulated = (Boolean) $insulated;
    }

    public function setServerParameters(array $server)
    {
        $this->server = array_merge(array(
            'HTTP_HOST'       => 'localhost',
            'HTTP_USER_AGENT' => 'Symfony2 BrowserKit',
        ), $server);
    }

    public function setServerParameter($key, $value)
    {
        $this->server[$key] = $value;
    }

    public function getServerParameter($key, $default = '')
    {
        return (isset($this->server[$key])) ? $this->server[$key] : $default;
    }

    public function getHistory()
    {
        return $this->history;
    }

    public function getCookieJar()
    {
        return $this->cookieJar;
    }

    public function getCrawler()
    {
        return $this->crawler;
    }

    public function getResponse()
    {
        return $this->response;
    }

    public function getRequest()
    {
        return $this->request;
    }

    public function click(Link $link)
    {
        if ($link instanceof Form) {
            return $this->submit($link);
        }

        return $this->request($link->getMethod(), $link->getUri());
    }

    public function submit(Form $form, array $values = array())
    {
        $form->setValues($values);

        return $this->request($form->getMethod(), $form->getUri(), $form->getPhpValues(), $form->getPhpFiles());
    }

    public function request($method, $uri, array $parameters = array(), array $files = array(), array $server = array(), $content = null, $changeHistory = true)
    {
        $uri = $this->getAbsoluteUri($uri);

        $server = array_merge($this->server, $server);
        if (!$this->history->isEmpty()) {
            $server['HTTP_REFERER'] = $this->history->current()->getUri();
        }
        $server['HTTP_HOST'] = parse_url($uri, PHP_URL_HOST);
        $server['HTTPS'] = 'https' == parse_url($uri, PHP_URL_SCHEME);

        $request = new Request($uri, $method, $parameters, $files, $this->cookieJar->allValues($uri), $server, $content);

        $this->request = $this->filterRequest($request);

        if (true === $changeHistory) {
            $this->history->add($request);
        }

        if ($this->insulated) {
            $this->response = $this->doRequestInProcess($this->request);
        } else {
            $this->response = $this->doRequest($this->request);
        }

        $response = $this->filterResponse($this->response);

        $this->cookieJar->updateFromResponse($response);

        $this->redirect = $response->getHeader('Location');

        if ($this->followRedirects && $this->redirect) {
            return $this->crawler = $this->followRedirect();
        }

        return $this->crawler = $this->createCrawlerFromContent($request->getUri(), $response->getContent(), $response->getHeader('Content-Type'));
    }

    protected function doRequestInProcess($request)
    {
        // We set the TMPDIR (for Macs) and TEMP (for Windows), because on these platforms the temp directory changes based on the user.
        $process = new PhpProcess($this->getScript($request), null, array('TMPDIR' => sys_get_temp_dir(), 'TEMP' => sys_get_temp_dir()));
        $process->run();

        if (!$process->isSuccessful() || !preg_match('/^O\:\d+\:/', $process->getOutput())) {
            throw new \RuntimeException('OUTPUT: '.$process->getOutput().' ERROR OUTPUT: '.$process->getErrorOutput());
        }

        return unserialize($process->getOutput());
    }

    abstract protected function doRequest($request);
    protected function getScript($request)
    {
        // @codeCoverageIgnoreStart
        throw new \LogicException('To insulate requests, you need to override the getScript() method.');
        // @codeCoverageIgnoreEnd
    }

    protected function filterRequest(Request $request)
    {
        return $request;
    }

    protected function filterResponse($response)
    {
        return $response;
    }

    protected function createCrawlerFromContent($uri, $content, $type)
    {
        if (!class_exists('Symfony\Component\DomCrawler\Crawler')) {
            return null;
        }

        $crawler = new Crawler(null, $uri);
        $crawler->addContent($content, $type);

        return $crawler;
    }

    public function back()
    {
        return $this->requestFromRequest($this->history->back(), false);
    }

    public function forward()
    {
        return $this->requestFromRequest($this->history->forward(), false);
    }

    public function reload()
    {
        return $this->requestFromRequest($this->history->current(), false);
    }

    public function followRedirect()
    {
        if (empty($this->redirect)) {
            throw new \LogicException('The request was not redirected.');
        }

        return $this->request('get', $this->redirect);
    }

    public function restart()
    {
        $this->cookieJar->clear();
        $this->history->clear();
    }

    protected function getAbsoluteUri($uri)
    {
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

    protected function requestFromRequest(Request $request, $changeHistory = true)
    {
        return $this->request($request->getMethod(), $request->getUri(), $request->getParameters(), $request->getFiles(), $request->getServer(), $request->getContent(), $changeHistory);
    }
}
