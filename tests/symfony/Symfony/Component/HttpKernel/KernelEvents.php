<?php

namespace Symfony\Component\HttpKernel;

final class KernelEvents
{
    const REQUEST = 'kernel.request';
    const EXCEPTION = 'kernel.exception';
    const VIEW = 'kernel.view';
    const CONTROLLER = 'kernel.controller';
    const RESPONSE = 'kernel.response';
    const TERMINATE = 'kernel.terminate';
}
