<?php

namespace Symfony\Component\Process;

class PhpProcess extends Process
{
    private $executableFinder;

    public function __construct($script, $cwd = null, array $env = array(), $timeout = 60, array $options = array())
    {
        parent::__construct(null, $cwd, $env, $script, $timeout, $options);

        $this->executableFinder = new PhpExecutableFinder();
    }

    public function setPhpBinary($php)
    {
        $this->setCommandLine($php);
    }

    public function run($callback = null)
    {
        if (null === $this->getCommandLine()) {
            if (false === $php = $this->executableFinder->find()) {
                throw new \RuntimeException('Unable to find the PHP executable.');
            }
            $this->setCommandLine($php);
        }

        return parent::run($callback);
    }
}
