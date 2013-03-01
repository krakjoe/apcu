<?php

namespace Symfony\Component\BrowserKit;

class Request
{
    protected $uri;
    protected $method;
    protected $parameters;
    protected $files;
    protected $cookies;
    protected $server;
    protected $content;

    public function __construct($uri, $method, array $parameters = array(), array $files = array(), array $cookies = array(), array $server = array(), $content = null)
    {
        $this->uri = $uri;
        $this->method = $method;
        $this->parameters = $parameters;
        $this->files = $files;
        $this->cookies = $cookies;
        $this->server = $server;
        $this->content = $content;
    }

    public function getUri()
    {
        return $this->uri;
    }

    public function getMethod()
    {
        return $this->method;
    }

    public function getParameters()
    {
        return $this->parameters;
    }

    public function getFiles()
    {
        return $this->files;
    }

    public function getCookies()
    {
        return $this->cookies;
    }

    public function getServer()
    {
        return $this->server;
    }

    public function getContent()
    {
        return $this->content;
    }
}
