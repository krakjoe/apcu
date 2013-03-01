<?php

namespace Symfony\Component\HttpFoundation;

class StreamedResponse extends Response
{
    protected $callback;
    protected $streamed;

    public function __construct($callback = null, $status = 200, $headers = array())
    {
        parent::__construct(null, $status, $headers);

        if (null !== $callback) {
            $this->setCallback($callback);
        }
        $this->streamed = false;
    }

    public static function create($callback = null, $status = 200, $headers = array())
    {
        return new static($callback, $status, $headers);
    }

    public function setCallback($callback)
    {
        if (!is_callable($callback)) {
            throw new \LogicException('The Response callback must be a valid PHP callable.');
        }
        $this->callback = $callback;
    }

    public function prepare(Request $request)
    {
        if ('1.0' != $request->server->get('SERVER_PROTOCOL')) {
            $this->setProtocolVersion('1.1');
        }

        $this->headers->set('Cache-Control', 'no-cache');

        return parent::prepare($request);
    }

    public function sendContent()
    {
        if ($this->streamed) {
            return;
        }

        $this->streamed = true;

        if (null === $this->callback) {
            throw new \LogicException('The Response callback must not be null.');
        }

        call_user_func($this->callback);
    }

    public function setContent($content)
    {
        if (null !== $content) {
            throw new \LogicException('The content cannot be set on a StreamedResponse instance.');
        }
    }

    public function getContent()
    {
        return false;
    }
}
