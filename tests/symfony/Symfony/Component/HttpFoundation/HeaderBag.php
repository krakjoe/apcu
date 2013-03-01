<?php

namespace Symfony\Component\HttpFoundation;

class HeaderBag implements \IteratorAggregate, \Countable
{
    protected $headers;
    protected $cacheControl;

    public function __construct(array $headers = array())
    {
        $this->cacheControl = array();
        $this->headers = array();
        foreach ($headers as $key => $values) {
            $this->set($key, $values);
        }
    }

    public function __toString()
    {
        if (!$this->headers) {
            return '';
        }

        $max = max(array_map('strlen', array_keys($this->headers))) + 1;
        $content = '';
        ksort($this->headers);
        foreach ($this->headers as $name => $values) {
            $name = implode('-', array_map('ucfirst', explode('-', $name)));
            foreach ($values as $value) {
                $content .= sprintf("%-{$max}s %s\r\n", $name.':', $value);
            }
        }

        return $content;
    }

    public function all()
    {
        return $this->headers;
    }

    public function keys()
    {
        return array_keys($this->headers);
    }

    public function replace(array $headers = array())
    {
        $this->headers = array();
        $this->add($headers);
    }

    public function add(array $headers)
    {
        foreach ($headers as $key => $values) {
            $this->set($key, $values);
        }
    }

    public function get($key, $default = null, $first = true)
    {
        $key = strtr(strtolower($key), '_', '-');

        if (!array_key_exists($key, $this->headers)) {
            if (null === $default) {
                return $first ? null : array();
            }

            return $first ? $default : array($default);
        }

        if ($first) {
            return count($this->headers[$key]) ? $this->headers[$key][0] : $default;
        }

        return $this->headers[$key];
    }

    public function set($key, $values, $replace = true)
    {
        $key = strtr(strtolower($key), '_', '-');

        $values = (array) $values;

        if (true === $replace || !isset($this->headers[$key])) {
            $this->headers[$key] = $values;
        } else {
            $this->headers[$key] = array_merge($this->headers[$key], $values);
        }

        if ('cache-control' === $key) {
            $this->cacheControl = $this->parseCacheControl($values[0]);
        }
    }

    public function has($key)
    {
        return array_key_exists(strtr(strtolower($key), '_', '-'), $this->headers);
    }

    public function contains($key, $value)
    {
        return in_array($value, $this->get($key, null, false));
    }

    public function remove($key)
    {
        $key = strtr(strtolower($key), '_', '-');

        unset($this->headers[$key]);

        if ('cache-control' === $key) {
            $this->cacheControl = array();
        }
    }

    public function getDate($key, \DateTime $default = null)
    {
        if (null === $value = $this->get($key)) {
            return $default;
        }

        if (false === $date = \DateTime::createFromFormat(DATE_RFC2822, $value)) {
            throw new \RuntimeException(sprintf('The %s HTTP header is not parseable (%s).', $key, $value));
        }

        return $date;
    }

    public function addCacheControlDirective($key, $value = true)
    {
        $this->cacheControl[$key] = $value;

        $this->set('Cache-Control', $this->getCacheControlHeader());
    }

    public function hasCacheControlDirective($key)
    {
        return array_key_exists($key, $this->cacheControl);
    }

    public function getCacheControlDirective($key)
    {
        return array_key_exists($key, $this->cacheControl) ? $this->cacheControl[$key] : null;
    }

    public function removeCacheControlDirective($key)
    {
        unset($this->cacheControl[$key]);

        $this->set('Cache-Control', $this->getCacheControlHeader());
    }

    public function getIterator()
    {
        return new \ArrayIterator($this->headers);
    }

    public function count()
    {
        return count($this->headers);
    }

    protected function getCacheControlHeader()
    {
        $parts = array();
        ksort($this->cacheControl);
        foreach ($this->cacheControl as $key => $value) {
            if (true === $value) {
                $parts[] = $key;
            } else {
                if (preg_match('#[^a-zA-Z0-9._-]#', $value)) {
                    $value = '"'.$value.'"';
                }

                $parts[] = "$key=$value";
            }
        }

        return implode(', ', $parts);
    }

    protected function parseCacheControl($header)
    {
        $cacheControl = array();
        preg_match_all('#([a-zA-Z][a-zA-Z_-]*)\s*(?:=(?:"([^"]*)"|([^ \t",;]*)))?#', $header, $matches, PREG_SET_ORDER);
        foreach ($matches as $match) {
            $cacheControl[strtolower($match[1])] = isset($match[2]) && $match[2] ? $match[2] : (isset($match[3]) ? $match[3] : true);
        }

        return $cacheControl;
    }
}
