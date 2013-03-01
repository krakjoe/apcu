<?php

namespace Symfony\Component\DomCrawler;

use Symfony\Component\CssSelector\CssSelector;

class Crawler extends \SplObjectStorage
{
    private $uri;

    public function __construct($node = null, $uri = null)
    {
        $this->uri = $uri;

        $this->add($node);
    }

    public function clear()
    {
        $this->removeAll($this);
    }

    public function add($node)
    {
        if ($node instanceof \DOMNodeList) {
            $this->addNodeList($node);
        } elseif (is_array($node)) {
            $this->addNodes($node);
        } elseif (is_string($node)) {
            $this->addContent($node);
        } elseif (is_object($node)) {
            $this->addNode($node);
        }
    }

    public function addContent($content, $type = null)
    {
        if (empty($type)) {
            $type = 'text/html';
        }

        // DOM only for HTML/XML content
        if (!preg_match('/(x|ht)ml/i', $type, $matches)) {
            return null;
        }

        $charset = 'ISO-8859-1';
        if (false !== $pos = strpos($type, 'charset=')) {
            $charset = substr($type, $pos + 8);
            if (false !== $pos = strpos($charset, ';')) {
                $charset = substr($charset, 0, $pos);
            }
        }

        if ('x' === $matches[1]) {
            $this->addXmlContent($content, $charset);
        } else {
            $this->addHtmlContent($content, $charset);
        }
    }

    public function addHtmlContent($content, $charset = 'UTF-8')
    {
        $dom = new \DOMDocument('1.0', $charset);
        $dom->validateOnParse = true;

        if (function_exists('mb_convert_encoding')) {
            $content = mb_convert_encoding($content, 'HTML-ENTITIES', $charset);
        }

        $current = libxml_use_internal_errors(true);
        @$dom->loadHTML($content);
        libxml_use_internal_errors($current);

        $this->addDocument($dom);

        $base = $this->filterXPath('descendant-or-self::base')->extract(array('href'));

        if (count($base)) {
            $this->uri = current($base);
        }
    }

    public function addXmlContent($content, $charset = 'UTF-8')
    {
        $dom = new \DOMDocument('1.0', $charset);
        $dom->validateOnParse = true;

        // remove the default namespace to make XPath expressions simpler
        $current = libxml_use_internal_errors(true);
        @$dom->loadXML(str_replace('xmlns', 'ns', $content));
        libxml_use_internal_errors($current);

        $this->addDocument($dom);
    }

    public function addDocument(\DOMDocument $dom)
    {
        if ($dom->documentElement) {
            $this->addNode($dom->documentElement);
        }
    }

    public function addNodeList(\DOMNodeList $nodes)
    {
        foreach ($nodes as $node) {
            $this->addNode($node);
        }
    }

    public function addNodes(array $nodes)
    {
        foreach ($nodes as $node) {
            $this->add($node);
        }
    }

    public function addNode(\DOMNode $node)
    {
        if ($node instanceof \DOMDocument) {
            $this->attach($node->documentElement);
        } else {
            $this->attach($node);
        }
    }

    public function eq($position)
    {
        foreach ($this as $i => $node) {
            if ($i == $position) {
                return new static($node, $this->uri);
            }
        }

        return new static(null, $this->uri);
    }

    public function each(\Closure $closure)
    {
        $data = array();
        foreach ($this as $i => $node) {
            $data[] = $closure($node, $i);
        }

        return $data;
    }

    public function reduce(\Closure $closure)
    {
        $nodes = array();
        foreach ($this as $i => $node) {
            if (false !== $closure($node, $i)) {
                $nodes[] = $node;
            }
        }

        return new static($nodes, $this->uri);
    }

    public function first()
    {
        return $this->eq(0);
    }

    public function last()
    {
        return $this->eq(count($this) - 1);
    }

    public function siblings()
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        return new static($this->sibling($this->getNode(0)->parentNode->firstChild), $this->uri);
    }

    public function nextAll()
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        return new static($this->sibling($this->getNode(0)), $this->uri);
    }

    public function previousAll()
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        return new static($this->sibling($this->getNode(0), 'previousSibling'), $this->uri);
    }

    public function parents()
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        $node = $this->getNode(0);
        $nodes = array();

        while ($node = $node->parentNode) {
            if (1 === $node->nodeType && '_root' !== $node->nodeName) {
                $nodes[] = $node;
            }
        }

        return new static($nodes, $this->uri);
    }

    public function children()
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        return new static($this->sibling($this->getNode(0)->firstChild), $this->uri);
    }

    public function attr($attribute)
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        return $this->getNode(0)->getAttribute($attribute);
    }

    public function text()
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        return $this->getNode(0)->nodeValue;
    }

    public function extract($attributes)
    {
        $attributes = (array) $attributes;

        $data = array();
        foreach ($this as $node) {
            $elements = array();
            foreach ($attributes as $attribute) {
                if ('_text' === $attribute) {
                    $elements[] = $node->nodeValue;
                } else {
                    $elements[] = $node->getAttribute($attribute);
                }
            }

            $data[] = count($attributes) > 1 ? $elements : $elements[0];
        }

        return $data;
    }

    public function filterXPath($xpath)
    {
        $document = new \DOMDocument('1.0', 'UTF-8');
        $root = $document->appendChild($document->createElement('_root'));
        foreach ($this as $node) {
            $root->appendChild($document->importNode($node, true));
        }

        $domxpath = new \DOMXPath($document);

        return new static($domxpath->query($xpath), $this->uri);
    }

    public function filter($selector)
    {
        if (!class_exists('Symfony\\Component\\CssSelector\\CssSelector')) {
            // @codeCoverageIgnoreStart
            throw new \RuntimeException('Unable to filter with a CSS selector as the Symfony CssSelector is not installed (you can use filterXPath instead).');
            // @codeCoverageIgnoreEnd
        }

        return $this->filterXPath(CssSelector::toXPath($selector));
    }

    public function selectLink($value)
    {
        $xpath  = sprintf('//a[contains(concat(\' \', normalize-space(string(.)), \' \'), %s)] ', static::xpathLiteral(' '.$value.' ')).
                            sprintf('| //a/img[contains(concat(\' \', normalize-space(string(@alt)), \' \'), %s)]/ancestor::a', static::xpathLiteral(' '.$value.' '));

        return $this->filterXPath($xpath);
    }

    public function selectButton($value)
    {
        $xpath = sprintf('//input[((@type="submit" or @type="button") and contains(concat(\' \', normalize-space(string(@value)), \' \'), %s)) ', static::xpathLiteral(' '.$value.' ')).
                         sprintf('or (@type="image" and contains(concat(\' \', normalize-space(string(@alt)), \' \'), %s)) or @id="%s" or @name="%s"] ', static::xpathLiteral(' '.$value.' '), $value, $value).
                         sprintf('| //button[contains(concat(\' \', normalize-space(string(.)), \' \'), %s) or @id="%s" or @name="%s"]', static::xpathLiteral(' '.$value.' '), $value, $value);

        return $this->filterXPath($xpath);
    }

    public function link($method = 'get')
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        $node = $this->getNode(0);

        return new Link($node, $this->uri, $method);
    }

    public function links()
    {
        $links = array();
        foreach ($this as $node) {
            $links[] = new Link($node, $this->uri, 'get');
        }

        return $links;
    }

    public function form(array $values = null, $method = null)
    {
        if (!count($this)) {
            throw new \InvalidArgumentException('The current node list is empty.');
        }

        $form = new Form($this->getNode(0), $this->uri, $method);

        if (null !== $values) {
            $form->setValues($values);
        }

        return $form;
    }

    public static function xpathLiteral($s)
    {
        if (false === strpos($s, "'")) {
            return sprintf("'%s'", $s);
        }

        if (false === strpos($s, '"')) {
            return sprintf('"%s"', $s);
        }

        $string = $s;
        $parts = array();
        while (true) {
            if (false !== $pos = strpos($string, "'")) {
                $parts[] = sprintf("'%s'", substr($string, 0, $pos));
                $parts[] = "\"'\"";
                $string = substr($string, $pos + 1);
            } else {
                $parts[] = "'$string'";
                break;
            }
        }

        return sprintf("concat(%s)", implode($parts, ', '));
    }

    private function getNode($position)
    {
        foreach ($this as $i => $node) {
            if ($i == $position) {
                return $node;
            }
        // @codeCoverageIgnoreStart
        }

        return null;
        // @codeCoverageIgnoreEnd
    }

    private function sibling($node, $siblingDir = 'nextSibling')
    {
        $nodes = array();

        do {
            if ($node !== $this->getNode(0) && $node->nodeType === 1) {
                $nodes[] = $node;
            }
        } while ($node = $node->$siblingDir);

        return $nodes;
    }
}
