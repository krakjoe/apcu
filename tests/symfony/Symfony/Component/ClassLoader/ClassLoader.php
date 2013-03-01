<?php

namespace Symfony\Component\ClassLoader;

class ClassLoader
{
    private $prefixes = array();
    private $fallbackDirs = array();
    private $useIncludePath = false;

    public function getPrefixes()
    {
        return $this->prefixes;
    }

    public function getFallbackDirs()
    {
        return $this->fallbackDirs;
    }

    public function addPrefixes(array $prefixes)
    {
        foreach ($prefixes as $prefix => $path) {
            $this->addPrefix($prefix, $path);
        }
    }

    public function addPrefix($prefix, $paths)
    {
        if (!$prefix) {
            foreach ((array) $paths as $path) {
                $this->fallbackDirs[] = $path;
            }

            return;
        }
        if (isset($this->prefixes[$prefix])) {
            $this->prefixes[$prefix] = array_merge(
                $this->prefixes[$prefix],
                (array) $paths
            );
        } else {
            $this->prefixes[$prefix] = (array) $paths;
        }
    }

    public function setUseIncludePath($useIncludePath)
    {
        $this->useIncludePath = $useIncludePath;
    }

    public function getUseIncludePath()
    {
        return $this->useIncludePath;
    }

    public function register($prepend = false)
    {
        spl_autoload_register(array($this, 'loadClass'), true, $prepend);
    }

    public function unregister()
    {
        spl_autoload_unregister(array($this, 'loadClass'));
    }

    public function loadClass($class)
    {
        if ($file = $this->findFile($class)) {
            require $file;

            return true;
        }
    }

    public function findFile($class)
    {
        if ('\\' == $class[0]) {
            $class = substr($class, 1);
        }

        if (false !== $pos = strrpos($class, '\\')) {
            // namespaced class name
            $classPath = str_replace('\\', DIRECTORY_SEPARATOR, substr($class, 0, $pos)) . DIRECTORY_SEPARATOR;
            $className = substr($class, $pos + 1);
        } else {
            // PEAR-like class name
            $classPath = null;
            $className = $class;
        }

        $classPath .= str_replace('_', DIRECTORY_SEPARATOR, $className) . '.php';

        foreach ($this->prefixes as $prefix => $dirs) {
            if (0 === strpos($class, $prefix)) {
                foreach ($dirs as $dir) {
                    if (file_exists($dir . DIRECTORY_SEPARATOR . $classPath)) {
                        return $dir . DIRECTORY_SEPARATOR . $classPath;
                    }
                }
            }
        }

        foreach ($this->fallbackDirs as $dir) {
            if (file_exists($dir . DIRECTORY_SEPARATOR . $classPath)) {
                return $dir . DIRECTORY_SEPARATOR . $classPath;
            }
        }

        if ($this->useIncludePath && $file = stream_resolve_include_path($classPath)) {
            return $file;
        }
    }
}
