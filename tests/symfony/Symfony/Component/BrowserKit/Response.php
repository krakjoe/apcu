<?php

namespace Symfony\Component\BrowserKit;

class Response
{
    protected $content;
    protected $status;
    protected $headers;

    public function __construct($content = '', $status = 200, array $headers = array())
    {
        $this->content = $content;
        $this->status  = $status;
        $this->headers = $headers;
    }

    public function __toString()
    {
        $headers = '';
        foreach ($this->headers as $name => $value) {
            if (is_string($value)) {
                $headers .= $this->buildHeader($name, $value);
            } else {
                foreach ($value as $headerValue) {
                    $headers .= $this->buildHeader($name, $headerValue);
                }
            }
        }

        return $headers."\n".$this->content;
    }

    protected function buildHeader($name, $value)
    {
        return sprintf("%s: %s\n", $name, $value);
    }

    public function getContent()
    {
        return $this->content;
    }

    public function getStatus()
    {
        return $this->status;
    }

    public function getHeaders()
    {
        return $this->headers;
    }

    public function getHeader($header, $first = true)
    {
        foreach ($this->headers as $key => $value) {
            if (str_replace('-', '_', strtolower($key)) == str_replace('-', '_', strtolower($header))) {
                if ($first) {
                    return is_array($value) ? (count($value) ? $value[0] : '') : $value;
                }

                return is_array($value) ? $value : array($value);
            }
        }

        return $first ? null : array();
    }
}
