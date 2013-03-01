<?php

namespace Symfony\Component\BrowserKit;

class CookieJar
{
    protected $cookieJar = array();

    public function set(Cookie $cookie)
    {
        $this->cookieJar[$cookie->getDomain()][$cookie->getPath()][$cookie->getName()] = $cookie;
    }

    public function get($name, $path = '/', $domain = null)
    {
        $this->flushExpiredCookies();

        return isset($this->cookieJar[$domain][$path][$name]) ? $this->cookieJar[$domain][$path][$name] : null;
    }

    public function expire($name, $path = '/', $domain = null)
    {
        if (null === $path) {
            $path = '/';
        }

        unset($this->cookieJar[$domain][$path][$name]);

        if (empty($this->cookieJar[$domain][$path])) {
            unset($this->cookieJar[$domain][$path]);

            if (empty($this->cookieJar[$domain])) {
                unset($this->cookieJar[$domain]);
            }
        }
    }

    public function clear()
    {
        $this->cookieJar = array();
    }

    public function updateFromSetCookie(array $setCookies, $uri = null)
    {
        $cookies = array();

        foreach ($setCookies as $cookie) {
            foreach (explode(',', $cookie) as $i => $part) {
                if (0 === $i || preg_match('/^(?P<token>\s*[0-9A-Za-z!#\$%\&\'\*\+\-\.^_`\|~]+)=/', $part)) {
                    $cookies[] = ltrim($part);
                } else {
                    $cookies[count($cookies) - 1] .= ','.$part;
                }
            }
        }

        foreach ($cookies as $cookie) {
            $this->set(Cookie::fromString($cookie, $uri));
        }
    }

    public function updateFromResponse(Response $response, $uri = null)
    {
        $this->updateFromSetCookie($response->getHeader('Set-Cookie', false), $uri);
    }

    public function all()
    {
        $this->flushExpiredCookies();

        $flattenedCookies = array();
        foreach ($this->cookieJar as $path) {
            foreach ($path as $cookies) {
                foreach ($cookies as $cookie) {
                    $flattenedCookies[] = $cookie;
                }
            }
        }

        return $flattenedCookies;
    }

    public function allValues($uri, $returnsRawValue = false)
    {
        $this->flushExpiredCookies();

        $parts = array_replace(array('path' => '/'), parse_url($uri));
        $cookies = array();
        foreach ($this->cookieJar as $domain => $pathCookies) {
            if ($domain) {
                $domain = ltrim($domain, '.');
                if ($domain != substr($parts['host'], -strlen($domain))) {
                    continue;
                }
            }

            foreach ($pathCookies as $path => $namedCookies) {
                if ($path != substr($parts['path'], 0, strlen($path))) {
                    continue;
                }

                foreach ($namedCookies as $cookie) {
                    if ($cookie->isSecure() && 'https' != $parts['scheme']) {
                        continue;
                    }

                    $cookies[$cookie->getName()] = $returnsRawValue ? $cookie->getRawValue() : $cookie->getValue();
                }
            }
        }

        return $cookies;
    }

    public function allRawValues($uri)
    {
        return $this->allValues($uri, true);
    }

    public function flushExpiredCookies()
    {
        foreach ($this->cookieJar as $domain => $pathCookies) {
            foreach ($pathCookies as $path => $namedCookies) {
                foreach ($namedCookies as $name => $cookie) {
                    if ($cookie->isExpired()) {
                        unset($this->cookieJar[$domain][$path][$name]);
                    }
                }
            }
        }
    }
}
