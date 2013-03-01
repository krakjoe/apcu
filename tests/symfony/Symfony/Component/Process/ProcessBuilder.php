<?php

namespace Symfony\Component\Process;

class ProcessBuilder
{
    private $arguments;
    private $cwd;
    private $env;
    private $stdin;
    private $timeout;
    private $options;
    private $inheritEnv;

    public function __construct(array $arguments = array())
    {
        $this->arguments = $arguments;

        $this->timeout = 60;
        $this->options = array();
        $this->env = array();
        $this->inheritEnv = true;
    }

    public static function create(array $arguments = array())
    {
        return new static($arguments);
    }

    public function add($argument)
    {
        $this->arguments[] = $argument;

        return $this;
    }

    public function setWorkingDirectory($cwd)
    {
        $this->cwd = $cwd;

        return $this;
    }

    public function inheritEnvironmentVariables($inheritEnv = true)
    {
        $this->inheritEnv = $inheritEnv;

        return $this;
    }

    public function setEnv($name, $value)
    {
        $this->env[$name] = $value;

        return $this;
    }

    public function setInput($stdin)
    {
        $this->stdin = $stdin;

        return $this;
    }

    public function setTimeout($timeout)
    {
        if (null === $timeout) {
            $this->timeout = null;

            return $this;
        }

        $timeout = (integer) $timeout;

        if ($timeout < 0) {
            throw new \InvalidArgumentException('The timeout value must be a valid positive integer.');
        }

        $this->timeout = $timeout;

        return $this;
    }

    public function setOption($name, $value)
    {
        $this->options[$name] = $value;

        return $this;
    }

    public function getProcess()
    {
        if (!count($this->arguments)) {
            throw new \LogicException('You must add() command arguments before calling getProcess().');
        }

        $options = $this->options;

        $script = implode(' ', array_map('escapeshellarg', $this->arguments));

        if ($this->inheritEnv) {
            $env = $this->env ? $this->env + $_ENV : null;
        } else {
            $env = $this->env;
        }

        return new Process($script, $this->cwd, $env, $this->stdin, $this->timeout, $options);
    }
}
