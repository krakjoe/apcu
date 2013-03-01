<?php

class PHPUnit_Framework_Constraint_IsTrue extends PHPUnit_Framework_Constraint
{
    protected function matches($other)
    {
        return $other === TRUE;
    }

    public function toString()
    {
        return 'is true';
    }
}
