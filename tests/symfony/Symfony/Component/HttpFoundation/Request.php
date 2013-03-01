<?php

namespace Symfony\Component\HttpFoundation;

use Symfony\Component\HttpFoundation\Session\SessionInterface;

class Request
{
    protected static $trustProxy = false;
    public $attributes;
    public $request;
    public $query;
    public $server;
    public $files;
    public $cookies;
    public $headers;
    protected $content;
    protected $languages;
    protected $charsets;
    protected $acceptableContentTypes;

    protected $pathInfo;
    protected $requestUri;
    protected $baseUrl;
    protected $basePath;
    protected $method;
    protected $format;
    protected $session;
    protected $locale;
    protected $defaultLocale = 'en';
    protected static $formats;

    public function __construct(array $query = array(), array $request = array(), array $attributes = array(), array $cookies = array(), array $files = array(), array $server = array(), $content = null)
    {
        $this->initialize($query, $request, $attributes, $cookies, $files, $server, $content);
    }

    public function initialize(array $query = array(), array $request = array(), array $attributes = array(), array $cookies = array(), array $files = array(), array $server = array(), $content = null)
    {
        $this->request = new ParameterBag($request);
        $this->query = new ParameterBag($query);
        $this->attributes = new ParameterBag($attributes);
        $this->cookies = new ParameterBag($cookies);
        $this->files = new FileBag($files);
        $this->server = new ServerBag($server);
        $this->headers = new HeaderBag($this->server->getHeaders());

        $this->content = $content;
        $this->languages = null;
        $this->charsets = null;
        $this->acceptableContentTypes = null;
        $this->pathInfo = null;
        $this->requestUri = null;
        $this->baseUrl = null;
        $this->basePath = null;
        $this->method = null;
        $this->format = null;
    }

    public static function createFromGlobals()
    {
        $request = new static($_GET, $_POST, array(), $_COOKIE, $_FILES, $_SERVER);

        if (0 === strpos($request->server->get('CONTENT_TYPE'), 'application/x-www-form-urlencoded')
            && in_array(strtoupper($request->server->get('REQUEST_METHOD', 'GET')), array('PUT', 'DELETE', 'PATCH'))
        ) {
            parse_str($request->getContent(), $data);
            $request->request = new ParameterBag($data);
        }

        return $request;
    }

    public static function create($uri, $method = 'GET', $parameters = array(), $cookies = array(), $files = array(), $server = array(), $content = null)
    {
        $defaults = array(
            'SERVER_NAME'          => 'localhost',
            'SERVER_PORT'          => 80,
            'HTTP_HOST'            => 'localhost',
            'HTTP_USER_AGENT'      => 'Symfony/2.X',
            'HTTP_ACCEPT'          => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'HTTP_ACCEPT_LANGUAGE' => 'en-us,en;q=0.5',
            'HTTP_ACCEPT_CHARSET'  => 'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
            'REMOTE_ADDR'          => '127.0.0.1',
            'SCRIPT_NAME'          => '',
            'SCRIPT_FILENAME'      => '',
            'SERVER_PROTOCOL'      => 'HTTP/1.1',
            'REQUEST_TIME'         => time(),
        );

        $components = parse_url($uri);
        if (isset($components['host'])) {
            $defaults['SERVER_NAME'] = $components['host'];
            $defaults['HTTP_HOST'] = $components['host'];
        }

        if (isset($components['scheme'])) {
            if ('https' === $components['scheme']) {
                $defaults['HTTPS'] = 'on';
                $defaults['SERVER_PORT'] = 443;
            }
        }

        if (isset($components['port'])) {
            $defaults['SERVER_PORT'] = $components['port'];
            $defaults['HTTP_HOST'] = $defaults['HTTP_HOST'].':'.$components['port'];
        }

        if (isset($components['user'])) {
            $defaults['PHP_AUTH_USER'] = $components['user'];
        }

        if (isset($components['pass'])) {
            $defaults['PHP_AUTH_PW'] = $components['pass'];
        }

        if (!isset($components['path'])) {
            $components['path'] = '';
        }

        switch (strtoupper($method)) {
            case 'POST':
            case 'PUT':
            case 'DELETE':
                $defaults['CONTENT_TYPE'] = 'application/x-www-form-urlencoded';
            case 'PATCH':
                $request = $parameters;
                $query = array();
                break;
            default:
                $request = array();
                $query = $parameters;
                break;
        }

        if (isset($components['query'])) {
            parse_str(html_entity_decode($components['query']), $qs);
            $query = array_replace($qs, $query);
        }
        $queryString = http_build_query($query, '', '&');

        $uri = $components['path'].('' !== $queryString ? '?'.$queryString : '');

        $server = array_replace($defaults, $server, array(
            'REQUEST_METHOD' => strtoupper($method),
            'PATH_INFO'      => '',
            'REQUEST_URI'    => $uri,
            'QUERY_STRING'   => $queryString,
        ));

        return new static($query, $request, array(), $cookies, $files, $server, $content);
    }

    public function duplicate(array $query = null, array $request = null, array $attributes = null, array $cookies = null, array $files = null, array $server = null)
    {
        $dup = clone $this;
        if ($query !== null) {
            $dup->query = new ParameterBag($query);
        }
        if ($request !== null) {
            $dup->request = new ParameterBag($request);
        }
        if ($attributes !== null) {
            $dup->attributes = new ParameterBag($attributes);
        }
        if ($cookies !== null) {
            $dup->cookies = new ParameterBag($cookies);
        }
        if ($files !== null) {
            $dup->files = new FileBag($files);
        }
        if ($server !== null) {
            $dup->server = new ServerBag($server);
            $dup->headers = new HeaderBag($dup->server->getHeaders());
        }
        $dup->languages = null;
        $dup->charsets = null;
        $dup->acceptableContentTypes = null;
        $dup->pathInfo = null;
        $dup->requestUri = null;
        $dup->baseUrl = null;
        $dup->basePath = null;
        $dup->method = null;
        $dup->format = null;

        return $dup;
    }

    public function __clone()
    {
        $this->query      = clone $this->query;
        $this->request    = clone $this->request;
        $this->attributes = clone $this->attributes;
        $this->cookies    = clone $this->cookies;
        $this->files      = clone $this->files;
        $this->server     = clone $this->server;
        $this->headers    = clone $this->headers;
    }

    public function __toString()
    {
        return
            sprintf('%s %s %s', $this->getMethod(), $this->getRequestUri(), $this->server->get('SERVER_PROTOCOL'))."\r\n".
            $this->headers."\r\n".
            $this->getContent();
    }

    public function overrideGlobals()
    {
        $_GET = $this->query->all();
        $_POST = $this->request->all();
        $_SERVER = $this->server->all();
        $_COOKIE = $this->cookies->all();

        foreach ($this->headers->all() as $key => $value) {
            $key = strtoupper(str_replace('-', '_', $key));
            if (in_array($key, array('CONTENT_TYPE', 'CONTENT_LENGTH'))) {
                $_SERVER[$key] = implode(', ', $value);
            } else {
                $_SERVER['HTTP_'.$key] = implode(', ', $value);
            }
        }

        $request = array('g' => $_GET, 'p' => $_POST, 'c' => $_COOKIE);

        $requestOrder = ini_get('request_order') ?: ini_get('variable_order');
        $requestOrder = preg_replace('#[^cgp]#', '', strtolower($requestOrder)) ?: 'gp';

        $_REQUEST = array();
        foreach (str_split($requestOrder) as $order) {
            $_REQUEST = array_merge($_REQUEST, $request[$order]);
        }
    }

    public static function trustProxyData()
    {
        self::$trustProxy = true;
    }

    public static function isProxyTrusted()
    {
        return self::$trustProxy;
    }

    public static function normalizeQueryString($qs)
    {
        if ('' == $qs) {
            return '';
        }

        $parts = array();
        $order = array();

        foreach (explode('&', $qs) as $param) {
            if ('' === $param || '=' === $param[0]) {
                // Ignore useless delimiters, e.g. "x=y&".
                // Also ignore pairs with empty key, even if there was a value, e.g. "=value", as such nameless values cannot be retrieved anyway.
                // PHP also does not include them when building _GET.
                continue;
            }

            $keyValuePair = explode('=', $param, 2);

            // GET parameters, that are submitted from a HTML form, encode spaces as "+" by default (as defined in enctype application/x-www-form-urlencoded).
            // PHP also converts "+" to spaces when filling the global _GET or when using the function parse_str. This is why we use urldecode and then normalize to
            // RFC 3986 with rawurlencode.
            $parts[] = isset($keyValuePair[1]) ?
                rawurlencode(urldecode($keyValuePair[0])).'='.rawurlencode(urldecode($keyValuePair[1])) :
                rawurlencode(urldecode($keyValuePair[0]));
            $order[] = urldecode($keyValuePair[0]);
        }

        array_multisort($order, SORT_ASC, $parts);

        return implode('&', $parts);
    }

    public function get($key, $default = null, $deep = false)
    {
        return $this->query->get($key, $this->attributes->get($key, $this->request->get($key, $default, $deep), $deep), $deep);
    }

    public function getSession()
    {
        return $this->session;
    }

    public function hasPreviousSession()
    {
        // the check for $this->session avoids malicious users trying to fake a session cookie with proper name
        return $this->hasSession() && $this->cookies->has($this->session->getName());
    }

    public function hasSession()
    {
        return null !== $this->session;
    }

    public function setSession(SessionInterface $session)
    {
        $this->session = $session;
    }

    public function getClientIp()
    {
        if (self::$trustProxy) {
            if ($this->server->has('HTTP_CLIENT_IP')) {
                return $this->server->get('HTTP_CLIENT_IP');
            } elseif ($this->server->has('HTTP_X_FORWARDED_FOR')) {
                $clientIp = explode(',', $this->server->get('HTTP_X_FORWARDED_FOR'));

                foreach ($clientIp as $ipAddress) {
                    $cleanIpAddress = trim($ipAddress);

                    if (false !== filter_var($cleanIpAddress, FILTER_VALIDATE_IP)) {
                        return $cleanIpAddress;
                    }
                }

                return '';
            }
        }

        return $this->server->get('REMOTE_ADDR');
    }

    public function getScriptName()
    {
        return $this->server->get('SCRIPT_NAME', $this->server->get('ORIG_SCRIPT_NAME', ''));
    }

    public function getPathInfo()
    {
        if (null === $this->pathInfo) {
            $this->pathInfo = $this->preparePathInfo();
        }

        return $this->pathInfo;
    }

    public function getBasePath()
    {
        if (null === $this->basePath) {
            $this->basePath = $this->prepareBasePath();
        }

        return $this->basePath;
    }

    public function getBaseUrl()
    {
        if (null === $this->baseUrl) {
            $this->baseUrl = $this->prepareBaseUrl();
        }

        return $this->baseUrl;
    }

    public function getScheme()
    {
        return $this->isSecure() ? 'https' : 'http';
    }

    public function getPort()
    {
        if (self::$trustProxy && $this->headers->has('X-Forwarded-Port')) {
            return $this->headers->get('X-Forwarded-Port');
        }

        return $this->server->get('SERVER_PORT');
    }

    public function getUser()
    {
        return $this->server->get('PHP_AUTH_USER');
    }

    public function getPassword()
    {
        return $this->server->get('PHP_AUTH_PW');
    }

    public function getUserInfo()
    {
        $userinfo = $this->getUser();

        $pass = $this->getPassword();
        if ('' != $pass) {
           $userinfo .= ":$pass";
        }

        return $userinfo;
    }

    public function getHttpHost()
    {
        $scheme = $this->getScheme();
        $port   = $this->getPort();

        if (('http' == $scheme && $port == 80) || ('https' == $scheme && $port == 443)) {
            return $this->getHost();
        }

        return $this->getHost().':'.$port;
    }

    public function getRequestUri()
    {
        if (null === $this->requestUri) {
            $this->requestUri = $this->prepareRequestUri();
        }

        return $this->requestUri;
    }

    public function getSchemeAndHttpHost()
    {
        return $this->getScheme().'://'.(('' != $auth = $this->getUserInfo()) ? $auth.'@' : '').$this->getHttpHost();
    }

    public function getUri()
    {
        $qs = $this->getQueryString();
        if (null !== $qs) {
            $qs = '?'.$qs;
        }

        return $this->getSchemeAndHttpHost().$this->getBaseUrl().$this->getPathInfo().$qs;
    }

    public function getUriForPath($path)
    {
        return $this->getSchemeAndHttpHost().$this->getBaseUrl().$path;
    }

    public function getQueryString()
    {
        $qs = static::normalizeQueryString($this->server->get('QUERY_STRING'));

        return '' === $qs ? null : $qs;
    }

    public function isSecure()
    {
        return (
            (strtolower($this->server->get('HTTPS')) == 'on' || $this->server->get('HTTPS') == 1)
            ||
            (self::$trustProxy && strtolower($this->headers->get('SSL_HTTPS')) == 'on' || $this->headers->get('SSL_HTTPS') == 1)
            ||
            (self::$trustProxy && strtolower($this->headers->get('X_FORWARDED_PROTO')) == 'https')
        );
    }

    public function getHost()
    {
        if (self::$trustProxy && $host = $this->headers->get('X_FORWARDED_HOST')) {
            $elements = explode(',', $host);

            $host = trim($elements[count($elements) - 1]);
        } else {
            if (!$host = $this->headers->get('HOST')) {
                if (!$host = $this->server->get('SERVER_NAME')) {
                    $host = $this->server->get('SERVER_ADDR', '');
                }
            }
        }

        // Remove port number from host
        $host = preg_replace('/:\d+$/', '', $host);

        // host is lowercase as per RFC 952/2181
        return trim(strtolower($host));
    }

    public function setMethod($method)
    {
        $this->method = null;
        $this->server->set('REQUEST_METHOD', $method);
    }

    public function getMethod()
    {
        if (null === $this->method) {
            $this->method = strtoupper($this->server->get('REQUEST_METHOD', 'GET'));
            if ('POST' === $this->method) {
                $this->method = strtoupper($this->headers->get('X-HTTP-METHOD-OVERRIDE', $this->request->get('_method', $this->query->get('_method', 'POST'))));
            }
        }

        return $this->method;
    }

    public function getMimeType($format)
    {
        if (null === static::$formats) {
            static::initializeFormats();
        }

        return isset(static::$formats[$format]) ? static::$formats[$format][0] : null;
    }

    public function getFormat($mimeType)
    {
        if (false !== $pos = strpos($mimeType, ';')) {
            $mimeType = substr($mimeType, 0, $pos);
        }

        if (null === static::$formats) {
            static::initializeFormats();
        }

        foreach (static::$formats as $format => $mimeTypes) {
            if (in_array($mimeType, (array) $mimeTypes)) {
                return $format;
            }
        }

        return null;
    }

    public function setFormat($format, $mimeTypes)
    {
        if (null === static::$formats) {
            static::initializeFormats();
        }

        static::$formats[$format] = is_array($mimeTypes) ? $mimeTypes : array($mimeTypes);
    }

    public function getRequestFormat($default = 'html')
    {
        if (null === $this->format) {
            $this->format = $this->get('_format', $default);
        }

        return $this->format;
    }

    public function setRequestFormat($format)
    {
        $this->format = $format;
    }

    public function getContentType()
    {
        return $this->getFormat($this->server->get('CONTENT_TYPE'));
    }

    public function setDefaultLocale($locale)
    {
        $this->setPhpDefaultLocale($this->defaultLocale = $locale);
    }

    public function setLocale($locale)
    {
        $this->setPhpDefaultLocale($this->locale = $locale);
    }

    public function getLocale()
    {
        return null === $this->locale ? $this->defaultLocale : $this->locale;
    }

    public function isMethod($method)
    {
        return $this->getMethod() === strtoupper($method);
    }

    public function isMethodSafe()
    {
        return in_array($this->getMethod(), array('GET', 'HEAD'));
    }

    public function getContent($asResource = false)
    {
        if (false === $this->content || (true === $asResource && null !== $this->content)) {
            throw new \LogicException('getContent() can only be called once when using the resource return type.');
        }

        if (true === $asResource) {
            $this->content = false;

            return fopen('php://input', 'rb');
        }

        if (null === $this->content) {
            $this->content = file_get_contents('php://input');
        }

        return $this->content;
    }

    public function getETags()
    {
        return preg_split('/\s*,\s*/', $this->headers->get('if_none_match'), null, PREG_SPLIT_NO_EMPTY);
    }

    public function isNoCache()
    {
        return $this->headers->hasCacheControlDirective('no-cache') || 'no-cache' == $this->headers->get('Pragma');
    }

    public function getPreferredLanguage(array $locales = null)
    {
        $preferredLanguages = $this->getLanguages();

        if (empty($locales)) {
            return isset($preferredLanguages[0]) ? $preferredLanguages[0] : null;
        }

        if (!$preferredLanguages) {
            return $locales[0];
        }

        $preferredLanguages = array_values(array_intersect($preferredLanguages, $locales));

        return isset($preferredLanguages[0]) ? $preferredLanguages[0] : $locales[0];
    }

    public function getLanguages()
    {
        if (null !== $this->languages) {
            return $this->languages;
        }

        $languages = $this->splitHttpAcceptHeader($this->headers->get('Accept-Language'));
        $this->languages = array();
        foreach ($languages as $lang => $q) {
            if (strstr($lang, '-')) {
                $codes = explode('-', $lang);
                if ($codes[0] == 'i') {
                    // Language not listed in ISO 639 that are not variants
                    // of any listed language, which can be registered with the
                    // i-prefix, such as i-cherokee
                    if (count($codes) > 1) {
                        $lang = $codes[1];
                    }
                } else {
                    for ($i = 0, $max = count($codes); $i < $max; $i++) {
                        if ($i == 0) {
                            $lang = strtolower($codes[0]);
                        } else {
                            $lang .= '_'.strtoupper($codes[$i]);
                        }
                    }
                }
            }

            $this->languages[] = $lang;
        }

        return $this->languages;
    }

    public function getCharsets()
    {
        if (null !== $this->charsets) {
            return $this->charsets;
        }

        return $this->charsets = array_keys($this->splitHttpAcceptHeader($this->headers->get('Accept-Charset')));
    }

    public function getAcceptableContentTypes()
    {
        if (null !== $this->acceptableContentTypes) {
            return $this->acceptableContentTypes;
        }

        return $this->acceptableContentTypes = array_keys($this->splitHttpAcceptHeader($this->headers->get('Accept')));
    }

    public function isXmlHttpRequest()
    {
        return 'XMLHttpRequest' == $this->headers->get('X-Requested-With');
    }

    public function splitHttpAcceptHeader($header)
    {
        if (!$header) {
            return array();
        }

        $values = array();
        foreach (array_filter(explode(',', $header)) as $value) {
            // Cut off any q-value that might come after a semi-colon
            if (preg_match('/;\s*(q=.*$)/', $value, $match)) {
                $q     = (float) substr(trim($match[1]), 2);
                $value = trim(substr($value, 0, -strlen($match[0])));
            } else {
                $q = 1;
            }

            if (0 < $q) {
                $values[trim($value)] = $q;
            }
        }

        arsort($values);
        reset($values);

        return $values;
    }

    protected function prepareRequestUri()
    {
        $requestUri = '';

        if ($this->headers->has('X_REWRITE_URL') && false !== stripos(PHP_OS, 'WIN')) {
            // check this first so IIS will catch
            $requestUri = $this->headers->get('X_REWRITE_URL');
        } elseif ($this->server->get('IIS_WasUrlRewritten') == '1' && $this->server->get('UNENCODED_URL') != '') {
            // IIS7 with URL Rewrite: make sure we get the unencoded url (double slash problem)
            $requestUri = $this->server->get('UNENCODED_URL');
        } elseif ($this->server->has('REQUEST_URI')) {
            $requestUri = $this->server->get('REQUEST_URI');
            // HTTP proxy reqs setup request uri with scheme and host [and port] + the url path, only use url path
            $schemeAndHttpHost = $this->getSchemeAndHttpHost();
            if (strpos($requestUri, $schemeAndHttpHost) === 0) {
                $requestUri = substr($requestUri, strlen($schemeAndHttpHost));
            }
        } elseif ($this->server->has('ORIG_PATH_INFO')) {
            // IIS 5.0, PHP as CGI
            $requestUri = $this->server->get('ORIG_PATH_INFO');
            if ('' != $this->server->get('QUERY_STRING')) {
                $requestUri .= '?'.$this->server->get('QUERY_STRING');
            }
        }

        return $requestUri;
    }

    protected function prepareBaseUrl()
    {
        $filename = basename($this->server->get('SCRIPT_FILENAME'));

        if (basename($this->server->get('SCRIPT_NAME')) === $filename) {
            $baseUrl = $this->server->get('SCRIPT_NAME');
        } elseif (basename($this->server->get('PHP_SELF')) === $filename) {
            $baseUrl = $this->server->get('PHP_SELF');
        } elseif (basename($this->server->get('ORIG_SCRIPT_NAME')) === $filename) {
            $baseUrl = $this->server->get('ORIG_SCRIPT_NAME'); // 1and1 shared hosting compatibility
        } else {
            // Backtrack up the script_filename to find the portion matching
            // php_self
            $path    = $this->server->get('PHP_SELF', '');
            $file    = $this->server->get('SCRIPT_FILENAME', '');
            $segs    = explode('/', trim($file, '/'));
            $segs    = array_reverse($segs);
            $index   = 0;
            $last    = count($segs);
            $baseUrl = '';
            do {
                $seg     = $segs[$index];
                $baseUrl = '/'.$seg.$baseUrl;
                ++$index;
            } while (($last > $index) && (false !== ($pos = strpos($path, $baseUrl))) && (0 != $pos));
        }

        // Does the baseUrl have anything in common with the request_uri?
        $requestUri = $this->getRequestUri();

        if ($baseUrl && false !== $prefix = $this->getUrlencodedPrefix($requestUri, $baseUrl)) {
            // full $baseUrl matches
            return $prefix;
        }

        if ($baseUrl && false !== $prefix = $this->getUrlencodedPrefix($requestUri, dirname($baseUrl))) {
            // directory portion of $baseUrl matches
            return rtrim($prefix, '/');
        }

        $truncatedRequestUri = $requestUri;
        if (($pos = strpos($requestUri, '?')) !== false) {
            $truncatedRequestUri = substr($requestUri, 0, $pos);
        }

        $basename = basename($baseUrl);
        if (empty($basename) || !strpos(rawurldecode($truncatedRequestUri), $basename)) {
            // no match whatsoever; set it blank
            return '';
        }

        // If using mod_rewrite or ISAPI_Rewrite strip the script filename
        // out of baseUrl. $pos !== 0 makes sure it is not matching a value
        // from PATH_INFO or QUERY_STRING
        if ((strlen($requestUri) >= strlen($baseUrl)) && ((false !== ($pos = strpos($requestUri, $baseUrl))) && ($pos !== 0))) {
            $baseUrl = substr($requestUri, 0, $pos + strlen($baseUrl));
        }

        return rtrim($baseUrl, '/');
    }

    protected function prepareBasePath()
    {
        $filename = basename($this->server->get('SCRIPT_FILENAME'));
        $baseUrl = $this->getBaseUrl();
        if (empty($baseUrl)) {
            return '';
        }

        if (basename($baseUrl) === $filename) {
            $basePath = dirname($baseUrl);
        } else {
            $basePath = $baseUrl;
        }

        if ('\\' === DIRECTORY_SEPARATOR) {
            $basePath = str_replace('\\', '/', $basePath);
        }

        return rtrim($basePath, '/');
    }

    protected function preparePathInfo()
    {
        $baseUrl = $this->getBaseUrl();

        if (null === ($requestUri = $this->getRequestUri())) {
            return '/';
        }

        $pathInfo = '/';

        // Remove the query string from REQUEST_URI
        if ($pos = strpos($requestUri, '?')) {
            $requestUri = substr($requestUri, 0, $pos);
        }

        if ((null !== $baseUrl) && (false === ($pathInfo = substr($requestUri, strlen($baseUrl))))) {
            // If substr() returns false then PATH_INFO is set to an empty string
            return '/';
        } elseif (null === $baseUrl) {
            return $requestUri;
        }

        return (string) $pathInfo;
    }

    protected static function initializeFormats()
    {
        static::$formats = array(
            'html' => array('text/html', 'application/xhtml+xml'),
            'txt'  => array('text/plain'),
            'js'   => array('application/javascript', 'application/x-javascript', 'text/javascript'),
            'css'  => array('text/css'),
            'json' => array('application/json', 'application/x-json'),
            'xml'  => array('text/xml', 'application/xml', 'application/x-xml'),
            'rdf'  => array('application/rdf+xml'),
            'atom' => array('application/atom+xml'),
            'rss'  => array('application/rss+xml'),
        );
    }

    private function setPhpDefaultLocale($locale)
    {
        // if either the class Locale doesn't exist, or an exception is thrown when
        // setting the default locale, the intl module is not installed, and
        // the call can be ignored:
        try {
            if (class_exists('Locale', false)) {
                \Locale::setDefault($locale);
            }
        } catch (\Exception $e) {
        }
    }

    private function getUrlencodedPrefix($string, $prefix)
    {
        if (0 !== strpos(rawurldecode($string), $prefix)) {
            return false;
        }

        $len = strlen($prefix);

        if (preg_match("#^(%[[:xdigit:]]{2}|.){{$len}}#", $string, $match)) {
            return $match[0];
        }

        return false;
    }
}
