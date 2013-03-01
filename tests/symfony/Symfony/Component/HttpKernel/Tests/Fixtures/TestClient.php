<?php

namespace Symfony\Component\HttpKernel\Tests\Fixtures;

use Symfony\Component\HttpKernel\Client;

class TestClient extends Client
{
    protected function getScript($request)
    {
        $script = parent::getScript($request);

        //$script = preg_replace('/(\->register\(\);)/', "$0\nrequire_once '".__DIR__."/../bootstrap.php';", $script);

        return $script;
    }
}
