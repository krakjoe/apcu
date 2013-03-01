<?php

namespace Symfony\Component\CssSelector;

use Symfony\Component\CssSelector\Exception\ParseException;

class CssSelector
{
    public static function toXPath($cssExpr, $prefix = 'descendant-or-self::')
    {
        if (is_string($cssExpr)) {
            if (!$cssExpr) {
                return $prefix.'*';
            }

            if (preg_match('#^\w+\s*$#u', $cssExpr, $match)) {
                return $prefix.trim($match[0]);
            }

            if (preg_match('~^(\w*)#(\w+)\s*$~u', $cssExpr, $match)) {
                return sprintf("%s%s[@id = '%s']", $prefix, $match[1] ? $match[1] : '*', $match[2]);
            }

            if (preg_match('#^(\w*)\.(\w+)\s*$#u', $cssExpr, $match)) {
                return sprintf("%s%s[contains(concat(' ', normalize-space(@class), ' '), ' %s ')]", $prefix, $match[1] ? $match[1] : '*', $match[2]);
            }

            $parser = new self();
            $cssExpr = $parser->parse($cssExpr);
        }

        $expr = $cssExpr->toXpath();

        // @codeCoverageIgnoreStart
        if (!$expr) {
            throw new ParseException(sprintf('Got None for xpath expression from %s.', $cssExpr));
        }
        // @codeCoverageIgnoreEnd

        if ($prefix) {
            $expr->addPrefix($prefix);
        }

        return (string) $expr;
    }

    public function parse($string)
    {
        $tokenizer = new Tokenizer();

        $stream = new TokenStream($tokenizer->tokenize($string), $string);

        try {
            return $this->parseSelectorGroup($stream);
        } catch (\Exception $e) {
            $class = get_class($e);

            throw new $class(sprintf('%s at %s -> %s', $e->getMessage(), implode($stream->getUsed(), ''), $stream->peek()), 0, $e);
        }
    }

    private function parseSelectorGroup($stream)
    {
        $result = array();
        while (true) {
            $result[] = $this->parseSelector($stream);
            if ($stream->peek() == ',') {
                $stream->next();
            } else {
                break;
            }
        }

        if (count($result) == 1) {
            return $result[0];
        }

        return new Node\OrNode($result);
    }

    private function parseSelector($stream)
    {
        $result = $this->parseSimpleSelector($stream);

        while (true) {
            $peek = $stream->peek();
            if (',' == $peek || null === $peek) {
                return $result;
            } elseif (in_array($peek, array('+', '>', '~'))) {
                // A combinator
                $combinator = (string) $stream->next();

                // Ignore optional whitespace after a combinator
                while (' ' == $stream->peek()) {
                    $stream->next();
                }
            } else {
                $combinator = ' ';
            }
            $consumed = count($stream->getUsed());
            $nextSelector = $this->parseSimpleSelector($stream);
            if ($consumed == count($stream->getUsed())) {
                throw new ParseException(sprintf("Expected selector, got '%s'", $stream->peek()));
            }

            $result = new Node\CombinedSelectorNode($result, $combinator, $nextSelector);
        }

        return $result;
    }

    private function parseSimpleSelector($stream)
    {
        $peek = $stream->peek();
        if ('*' != $peek && !$peek->isType('Symbol')) {
            $element = $namespace = '*';
        } else {
            $next = $stream->next();
            if ('*' != $next && !$next->isType('Symbol')) {
                throw new ParseException(sprintf("Expected symbol, got '%s'", $next));
            }

            if ($stream->peek() == '|') {
                $namespace = $next;
                $stream->next();
                $element = $stream->next();
                if ('*' != $element && !$next->isType('Symbol')) {
                    throw new ParseException(sprintf("Expected symbol, got '%s'", $next));
                }
            } else {
                $namespace = '*';
                $element = $next;
            }
        }

        $result = new Node\ElementNode($namespace, $element);
        $hasHash = false;
        while (true) {
            $peek = $stream->peek();
            if ('#' == $peek) {
                if ($hasHash) {
                    /* You can't have two hashes
                        (FIXME: is there some more general rule I'm missing?) */
                    // @codeCoverageIgnoreStart
                    break;
                    // @codeCoverageIgnoreEnd
                }
                $stream->next();
                $result = new Node\HashNode($result, $stream->next());
                $hasHash = true;

                continue;
            } elseif ('.' == $peek) {
                $stream->next();
                $result = new Node\ClassNode($result, $stream->next());

                continue;
            } elseif ('[' == $peek) {
                $stream->next();
                $result = $this->parseAttrib($result, $stream);
                $next = $stream->next();
                if (']' != $next) {
                    throw new ParseException(sprintf("] expected, got '%s'", $next));
                }

                continue;
            } elseif (':' == $peek || '::' == $peek) {
                $type = $stream->next();
                $ident = $stream->next();
                if (!$ident || !$ident->isType('Symbol')) {
                    throw new ParseException(sprintf("Expected symbol, got '%s'", $ident));
                }

                if ($stream->peek() == '(') {
                    $stream->next();
                    $peek = $stream->peek();
                    if ($peek->isType('String')) {
                        $selector = $stream->next();
                    } elseif ($peek->isType('Symbol') && is_int($peek)) {
                        $selector = intval($stream->next());
                    } else {
                        // FIXME: parseSimpleSelector, or selector, or...?
                        $selector = $this->parseSimpleSelector($stream);
                    }
                    $next = $stream->next();
                    if (')' != $next) {
                        throw new ParseException(sprintf("Expected ')', got '%s' and '%s'", $next, $selector));
                    }

                    $result = new Node\FunctionNode($result, $type, $ident, $selector);
                } else {
                    $result = new Node\PseudoNode($result, $type, $ident);
                }

                continue;
            } else {
                if (' ' == $peek) {
                    $stream->next();
                }

                break;
            }
            // FIXME: not sure what "negation" is
        }

        return $result;
    }

    private function parseAttrib($selector, $stream)
    {
        $attrib = $stream->next();
        if ($stream->peek() == '|') {
            $namespace = $attrib;
            $stream->next();
            $attrib = $stream->next();
        } else {
            $namespace = '*';
        }

        if ($stream->peek() == ']') {
            return new Node\AttribNode($selector, $namespace, $attrib, 'exists', null);
        }

        $op = $stream->next();
        if (!in_array($op, array('^=', '$=', '*=', '=', '~=', '|=', '!='))) {
            throw new ParseException(sprintf("Operator expected, got '%s'", $op));
        }

        $value = $stream->next();
        if (!$value->isType('Symbol') && !$value->isType('String')) {
            throw new ParseException(sprintf("Expected string or symbol, got '%s'", $value));
        }

        return new Node\AttribNode($selector, $namespace, $attrib, $op, $value);
    }
}
