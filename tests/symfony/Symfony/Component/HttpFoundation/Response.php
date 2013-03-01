<?php

namespace Symfony\Component\HttpFoundation;

class Response
{
    public $headers;

    protected $content;
    protected $version;
    protected $statusCode;
    protected $statusText;
    protected $charset;

    public static $statusTexts = array(
        100 => 'Continue',
        101 => 'Switching Protocols',
        102 => 'Processing',            // RFC2518
        200 => 'OK',
        201 => 'Created',
        202 => 'Accepted',
        203 => 'Non-Authoritative Information',
        204 => 'No Content',
        205 => 'Reset Content',
        206 => 'Partial Content',
        207 => 'Multi-Status',          // RFC4918
        208 => 'Already Reported',      // RFC5842
        226 => 'IM Used',               // RFC3229
        300 => 'Multiple Choices',
        301 => 'Moved Permanently',
        302 => 'Found',
        303 => 'See Other',
        304 => 'Not Modified',
        305 => 'Use Proxy',
        306 => 'Reserved',
        307 => 'Temporary Redirect',
        308 => 'Permanent Redirect',    // RFC-reschke-http-status-308-07
        400 => 'Bad Request',
        401 => 'Unauthorized',
        402 => 'Payment Required',
        403 => 'Forbidden',
        404 => 'Not Found',
        405 => 'Method Not Allowed',
        406 => 'Not Acceptable',
        407 => 'Proxy Authentication Required',
        408 => 'Request Timeout',
        409 => 'Conflict',
        410 => 'Gone',
        411 => 'Length Required',
        412 => 'Precondition Failed',
        413 => 'Request Entity Too Large',
        414 => 'Request-URI Too Long',
        415 => 'Unsupported Media Type',
        416 => 'Requested Range Not Satisfiable',
        417 => 'Expectation Failed',
        418 => 'I\'m a teapot',                                               // RFC2324
        422 => 'Unprocessable Entity',                                        // RFC4918
        423 => 'Locked',                                                      // RFC4918
        424 => 'Failed Dependency',                                           // RFC4918
        425 => 'Reserved for WebDAV advanced collections expired proposal',   // RFC2817
        426 => 'Upgrade Required',                                            // RFC2817
        428 => 'Precondition Required',                                       // RFC6585
        429 => 'Too Many Requests',                                           // RFC6585
        431 => 'Request Header Fields Too Large',                             // RFC6585
        500 => 'Internal Server Error',
        501 => 'Not Implemented',
        502 => 'Bad Gateway',
        503 => 'Service Unavailable',
        504 => 'Gateway Timeout',
        505 => 'HTTP Version Not Supported',
        506 => 'Variant Also Negotiates (Experimental)',                      // RFC2295
        507 => 'Insufficient Storage',                                        // RFC4918
        508 => 'Loop Detected',                                               // RFC5842
        510 => 'Not Extended',                                                // RFC2774
        511 => 'Network Authentication Required',                             // RFC6585
    );

    public function __construct($content = '', $status = 200, $headers = array())
    {
        $this->headers = new ResponseHeaderBag($headers);
        $this->setContent($content);
        $this->setStatusCode($status);
        $this->setProtocolVersion('1.0');
        if (!$this->headers->has('Date')) {
            $this->setDate(new \DateTime(null, new \DateTimeZone('UTC')));
        }
    }

    public static function create($content = '', $status = 200, $headers = array())
    {
        return new static($content, $status, $headers);
    }

    public function __toString()
    {
        return
            sprintf('HTTP/%s %s %s', $this->version, $this->statusCode, $this->statusText)."\r\n".
            $this->headers."\r\n".
            $this->getContent();
    }

    public function __clone()
    {
        $this->headers = clone $this->headers;
    }

    public function prepare(Request $request)
    {
        $headers = $this->headers;

        if ($this->isInformational() || in_array($this->statusCode, array(204, 304))) {
            $this->setContent(null);
        }

        // Content-type based on the Request
        if (!$headers->has('Content-Type')) {
            $format = $request->getRequestFormat();
            if (null !== $format && $mimeType = $request->getMimeType($format)) {
                $headers->set('Content-Type', $mimeType);
            }
        }

        // Fix Content-Type
        $charset = $this->charset ?: 'UTF-8';
        if (!$headers->has('Content-Type')) {
            $headers->set('Content-Type', 'text/html; charset='.$charset);
        } elseif (0 === strpos($headers->get('Content-Type'), 'text/') && false === strpos($headers->get('Content-Type'), 'charset')) {
            // add the charset
            $headers->set('Content-Type', $headers->get('Content-Type').'; charset='.$charset);
        }

        // Fix Content-Length
        if ($headers->has('Transfer-Encoding')) {
            $headers->remove('Content-Length');
        }

        if ('HEAD' === $request->getMethod()) {
            // cf. RFC2616 14.13
            $length = $headers->get('Content-Length');
            $this->setContent(null);
            if ($length) {
                $headers->set('Content-Length', $length);
            }
        }

        return $this;
    }

    public function sendHeaders()
    {
        // headers have already been sent by the developer
        if (headers_sent()) {
            return $this;
        }

        // status
        header(sprintf('HTTP/%s %s %s', $this->version, $this->statusCode, $this->statusText));

        // headers
        foreach ($this->headers->all() as $name => $values) {
            foreach ($values as $value) {
                header($name.': '.$value, false);
            }
        }

        // cookies
        foreach ($this->headers->getCookies() as $cookie) {
            setcookie($cookie->getName(), $cookie->getValue(), $cookie->getExpiresTime(), $cookie->getPath(), $cookie->getDomain(), $cookie->isSecure(), $cookie->isHttpOnly());
        }

        return $this;
    }

    public function sendContent()
    {
        echo $this->content;

        return $this;
    }

    public function send()
    {
        $this->sendHeaders();
        $this->sendContent();

        if (function_exists('fastcgi_finish_request')) {
            fastcgi_finish_request();
        } elseif ('cli' !== PHP_SAPI) {
            // ob_get_level() never returns 0 on some Windows configurations, so if
            // the level is the same two times in a row, the loop should be stopped.
            $previous = null;
            while (($level = ob_get_level()) > 0 && $level !== $previous) {
                $previous = $level;
                ob_end_flush();
            }
            flush();
        }

        return $this;
    }

    public function setContent($content)
    {
        if (null !== $content && !is_string($content) && !is_numeric($content) && !is_callable(array($content, '__toString'))) {
            throw new \UnexpectedValueException('The Response content must be a string or object implementing __toString(), "'.gettype($content).'" given.');
        }

        $this->content = (string) $content;

        return $this;
    }

    public function getContent()
    {
        return $this->content;
    }

    public function setProtocolVersion($version)
    {
        $this->version = $version;

        return $this;
    }

    public function getProtocolVersion()
    {
        return $this->version;
    }

    public function setStatusCode($code, $text = null)
    {
        $this->statusCode = $code = (int) $code;
        if ($this->isInvalid()) {
            throw new \InvalidArgumentException(sprintf('The HTTP status code "%s" is not valid.', $code));
        }

        if (null === $text) {
            $this->statusText = isset(self::$statusTexts[$code]) ? self::$statusTexts[$code] : '';

            return $this;
        }

        if (false === $text) {
            $this->statusText = '';

            return $this;
        }

        $this->statusText = $text;

        return $this;
    }

    public function getStatusCode()
    {
        return $this->statusCode;
    }

    public function setCharset($charset)
    {
        $this->charset = $charset;

        return $this;
    }

    public function getCharset()
    {
        return $this->charset;
    }

    public function isCacheable()
    {
        if (!in_array($this->statusCode, array(200, 203, 300, 301, 302, 404, 410))) {
            return false;
        }

        if ($this->headers->hasCacheControlDirective('no-store') || $this->headers->getCacheControlDirective('private')) {
            return false;
        }

        return $this->isValidateable() || $this->isFresh();
    }

    public function isFresh()
    {
        return $this->getTtl() > 0;
    }

    public function isValidateable()
    {
        return $this->headers->has('Last-Modified') || $this->headers->has('ETag');
    }

    public function setPrivate()
    {
        $this->headers->removeCacheControlDirective('public');
        $this->headers->addCacheControlDirective('private');

        return $this;
    }

    public function setPublic()
    {
        $this->headers->addCacheControlDirective('public');
        $this->headers->removeCacheControlDirective('private');

        return $this;
    }

    public function mustRevalidate()
    {
        return $this->headers->hasCacheControlDirective('must-revalidate') || $this->headers->has('proxy-revalidate');
    }

    public function getDate()
    {
        return $this->headers->getDate('Date');
    }

    public function setDate(\DateTime $date)
    {
        $date->setTimezone(new \DateTimeZone('UTC'));
        $this->headers->set('Date', $date->format('D, d M Y H:i:s').' GMT');

        return $this;
    }

    public function getAge()
    {
        if ($age = $this->headers->get('Age')) {
            return $age;
        }

        return max(time() - $this->getDate()->format('U'), 0);
    }

    public function expire()
    {
        if ($this->isFresh()) {
            $this->headers->set('Age', $this->getMaxAge());
        }

        return $this;
    }

    public function getExpires()
    {
        return $this->headers->getDate('Expires');
    }

    public function setExpires(\DateTime $date = null)
    {
        if (null === $date) {
            $this->headers->remove('Expires');
        } else {
            $date = clone $date;
            $date->setTimezone(new \DateTimeZone('UTC'));
            $this->headers->set('Expires', $date->format('D, d M Y H:i:s').' GMT');
        }

        return $this;
    }

    public function getMaxAge()
    {
        if ($age = $this->headers->getCacheControlDirective('s-maxage')) {
            return $age;
        }

        if ($age = $this->headers->getCacheControlDirective('max-age')) {
            return $age;
        }

        if (null !== $this->getExpires()) {
            return $this->getExpires()->format('U') - $this->getDate()->format('U');
        }

        return null;
    }

    public function setMaxAge($value)
    {
        $this->headers->addCacheControlDirective('max-age', $value);

        return $this;
    }

    public function setSharedMaxAge($value)
    {
        $this->setPublic();
        $this->headers->addCacheControlDirective('s-maxage', $value);

        return $this;
    }

    public function getTtl()
    {
        if ($maxAge = $this->getMaxAge()) {
            return $maxAge - $this->getAge();
        }

        return null;
    }

    public function setTtl($seconds)
    {
        $this->setSharedMaxAge($this->getAge() + $seconds);

        return $this;
    }

    public function setClientTtl($seconds)
    {
        $this->setMaxAge($this->getAge() + $seconds);

        return $this;
    }

    public function getLastModified()
    {
        return $this->headers->getDate('Last-Modified');
    }

    public function setLastModified(\DateTime $date = null)
    {
        if (null === $date) {
            $this->headers->remove('Last-Modified');
        } else {
            $date = clone $date;
            $date->setTimezone(new \DateTimeZone('UTC'));
            $this->headers->set('Last-Modified', $date->format('D, d M Y H:i:s').' GMT');
        }

        return $this;
    }

    public function getEtag()
    {
        return $this->headers->get('ETag');
    }

    public function setEtag($etag = null, $weak = false)
    {
        if (null === $etag) {
            $this->headers->remove('Etag');
        } else {
            if (0 !== strpos($etag, '"')) {
                $etag = '"'.$etag.'"';
            }

            $this->headers->set('ETag', (true === $weak ? 'W/' : '').$etag);
        }

        return $this;
    }

    public function setCache(array $options)
    {
        if ($diff = array_diff(array_keys($options), array('etag', 'last_modified', 'max_age', 's_maxage', 'private', 'public'))) {
            throw new \InvalidArgumentException(sprintf('Response does not support the following options: "%s".', implode('", "', array_values($diff))));
        }

        if (isset($options['etag'])) {
            $this->setEtag($options['etag']);
        }

        if (isset($options['last_modified'])) {
            $this->setLastModified($options['last_modified']);
        }

        if (isset($options['max_age'])) {
            $this->setMaxAge($options['max_age']);
        }

        if (isset($options['s_maxage'])) {
            $this->setSharedMaxAge($options['s_maxage']);
        }

        if (isset($options['public'])) {
            if ($options['public']) {
                $this->setPublic();
            } else {
                $this->setPrivate();
            }
        }

        if (isset($options['private'])) {
            if ($options['private']) {
                $this->setPrivate();
            } else {
                $this->setPublic();
            }
        }

        return $this;
    }

    public function setNotModified()
    {
        $this->setStatusCode(304);
        $this->setContent(null);

        // remove headers that MUST NOT be included with 304 Not Modified responses
        foreach (array('Allow', 'Content-Encoding', 'Content-Language', 'Content-Length', 'Content-MD5', 'Content-Type', 'Last-Modified') as $header) {
            $this->headers->remove($header);
        }

        return $this;
    }

    public function hasVary()
    {
        return (Boolean) $this->headers->get('Vary');
    }

    public function getVary()
    {
        if (!$vary = $this->headers->get('Vary')) {
            return array();
        }

        return is_array($vary) ? $vary : preg_split('/[\s,]+/', $vary);
    }

    public function setVary($headers, $replace = true)
    {
        $this->headers->set('Vary', $headers, $replace);

        return $this;
    }

    public function isNotModified(Request $request)
    {
        if (!$request->isMethodSafe()) {
            return false;
        }

        $lastModified = $request->headers->get('If-Modified-Since');
        $notModified = false;
        if ($etags = $request->getEtags()) {
            $notModified = (in_array($this->getEtag(), $etags) || in_array('*', $etags)) && (!$lastModified || $this->headers->get('Last-Modified') == $lastModified);
        } elseif ($lastModified) {
            $notModified = $lastModified == $this->headers->get('Last-Modified');
        }

        if ($notModified) {
            $this->setNotModified();
        }

        return $notModified;
    }

    public function isInvalid()
    {
        return $this->statusCode < 100 || $this->statusCode >= 600;
    }

    public function isInformational()
    {
        return $this->statusCode >= 100 && $this->statusCode < 200;
    }

    public function isSuccessful()
    {
        return $this->statusCode >= 200 && $this->statusCode < 300;
    }

    public function isRedirection()
    {
        return $this->statusCode >= 300 && $this->statusCode < 400;
    }

    public function isClientError()
    {
        return $this->statusCode >= 400 && $this->statusCode < 500;
    }

    public function isServerError()
    {
        return $this->statusCode >= 500 && $this->statusCode < 600;
    }

    public function isOk()
    {
        return 200 === $this->statusCode;
    }

    public function isForbidden()
    {
        return 403 === $this->statusCode;
    }

    public function isNotFound()
    {
        return 404 === $this->statusCode;
    }

    public function isRedirect($location = null)
    {
        return in_array($this->statusCode, array(201, 301, 302, 303, 307, 308)) && (null === $location ?: $location == $this->headers->get('Location'));
    }

    public function isEmpty()
    {
        return in_array($this->statusCode, array(201, 204, 304));
    }
}
