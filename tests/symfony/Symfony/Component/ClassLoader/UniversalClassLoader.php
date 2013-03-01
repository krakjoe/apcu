<?php

namespace Symfony\Component\ClassLoader;

class UniversalClassLoader
{
    private $namespaces = array();
    private $prefixes = array();
    private $namespaceFallbacks = array();
    private $prefixFallbacks = array();
    private $useIncludePath = false;

    public function useIncludePath($useIncludePath)
    {
        $this->useIncludePath = $useIncludePath;
    }

    public function getUseIncludePath()
    {
        return $this->useIncludePath;
    }

    public function getNamespaces()
    {
        return $this->namespaces;
    }

    public function getPrefixes()
    {
        return $this->prefixes;
    }

    public function getNamespaceFallbacks()
    {
        return $this->namespaceFallbacks;
    }

    public function getPrefixFallbacks()
    {
        return $this->prefixFallbacks;
    }

    public function registerNamespaceFallbacks(array $dirs)
    {
        $this->namespaceFallbacks = $dirs;
    }

    public function registerNamespaceFallback($dir)
    {
        $this->namespaceFallbacks[] = $dir;
    }

    public function registerPrefixFallbacks(array $dirs)
    {
        $this->prefixFallbacks = $dirs;
    }

    public function registerPrefixFallback($dir)
    {
        $this->prefixFallbacks[] = $dir;
    }

    public function registerNamespaces(array $namespaces)
    {
        foreach ($namespaces as $namespace => $locations) {
            $this->namespaces[$namespace] = (array) $locations;
        }
    }

    public function registerNamespace($namespace, $paths)
    {
        $this->namespaces[$namespace] = (array) $paths;
    }

    public function registerPrefixes(array $classes)
    {
        foreach ($classes as $prefix => $locations) {
            $this->prefixes[$prefix] = (array) $locations;
        }
    }

    public function registerPrefix($prefix, $paths)
    {
        $this->prefixes[$prefix] = (array) $paths;
    }

    public function register($prepend = false)
    {
        spl_autoload_register(array($this, 'loadClass'), true, $prepend);
    }

    public function loadClass($class)
    {
        if ($file = $this->findFile($class)) {
            require $file;
        }
    }

    public function findFile($class)
    {
        if ('\\' == $class[0]) {
            $class = substr($class, 1);
        }

        if (false !== $pos = strrpos($class, '\\')) {
            // namespaced class name
            $namespace = substr($class, 0, $pos);
            $className = substr($class, $pos + 1);
            $normalizedClass = str_replace('\\', DIRECTORY_SEPARATOR, $namespace).DIRECTORY_SEPARATOR.str_replace('_', DIRECTORY_SEPARATOR, $className).'.php';
            foreach ($this->namespaces as $ns => $dirs) {
                if (0 !== strpos($namespace, $ns)) {
                    continue;
                }

                foreach ($dirs as $dir) {
                    $file = $dir.DIRECTORY_SEPARATOR.$normalizedClass;
                    if (is_file($file)) {
                        return $file;
                    }
                }
            }

            foreach ($this->namespaceFallbacks as $dir) {
                $file = $dir.DIRECTORY_SEPARATOR.$normalizedClass;
                if (is_file($file)) {
                    return $file;
                }
            }

        } else {
            // PEAR-like class name
            $normalizedClass = str_replace('_', DIRECTORY_SEPARATOR, $class).'.php';
            foreach ($this->prefixes as $prefix => $dirs) {
                if (0 !== strpos($class, $prefix)) {
                    continue;
                }

                foreach ($dirs as $dir) {
                    $file = $dir.DIRECTORY_SEPARATOR.$normalizedClass;
                    if (is_file($file)) {
                        return $file;
                    }
                }
            }

            foreach ($this->prefixFallbacks as $dir) {
                $file = $dir.DIRECTORY_SEPARATOR.$normalizedClass;
                if (is_file($file)) {
                    return $file;
                }
            }
        }

        if ($this->useIncludePath && $file = stream_resolve_include_path($normalizedClass)) {
            return $file;
        }
    }
}
