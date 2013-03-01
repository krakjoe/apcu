<?php

abstract class PHPUnit_Framework_Constraint implements Countable, PHPUnit_Framework_SelfDescribing
{

    public function evaluate($other, $description = '', $returnResult = FALSE)
    {
        $success = FALSE;

        if ($this->matches($other)) {
            $success = TRUE;
        }

        if ($returnResult) {
            return $success;
        }

        if (!$success) {
            $this->fail($other, $description);
        }
    }

    protected function matches($other)
    {
        return FALSE;
    }

    public function count()
    {
        return 1;
    }

    protected function fail($other, $description, PHPUnit_Framework_ComparisonFailure $comparisonFailure = NULL)
    {
        $failureDescription = sprintf(
          'Failed asserting that %s.',
          $this->failureDescription($other)
        );

        $additionalFailureDescription = $this->additionalFailureDescription($other);
        if ($additionalFailureDescription) {
            $failureDescription .= "\n" . $additionalFailureDescription;
        }

        if (!empty($description)) {
            $failureDescription = $description . "\n" . $failureDescription;
        }

        throw new PHPUnit_Framework_ExpectationFailedException(
          $failureDescription,
          $comparisonFailure
        );
    }

    protected function additionalFailureDescription($other)
    {
        return "";
    }

    protected function failureDescription($other)
    {
        return PHPUnit_Util_Type::export($other) . ' ' . $this->toString();
    }
}
