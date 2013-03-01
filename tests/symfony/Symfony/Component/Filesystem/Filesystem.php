<?php

namespace Symfony\Component\Filesystem;

use Symfony\Component\Filesystem\Exception\IOException;

class Filesystem
{
    public function copy($originFile, $targetFile, $override = false)
    {
        $this->mkdir(dirname($targetFile));

        if (!$override && is_file($targetFile)) {
            $doCopy = filemtime($originFile) > filemtime($targetFile);
        } else {
            $doCopy = true;
        }

        if ($doCopy) {
            if (true !== @copy($originFile, $targetFile)) {
                throw new IOException(sprintf('Failed to copy %s to %s', $originFile, $targetFile));
            }
        }
    }

    public function mkdir($dirs, $mode = 0777)
    {
        foreach ($this->toIterator($dirs) as $dir) {
            if (is_dir($dir)) {
                continue;
            }

            if (true !== @mkdir($dir, $mode, true)) {
                throw new IOException(sprintf('Failed to create %s', $dir));
            }
        }
    }

    public function exists($files)
    {
        foreach ($this->toIterator($files) as $file) {
            if (!file_exists($file)) {
                return false;
            }
        }

        return true;
    }

    public function touch($files, $time = null, $atime = null)
    {
        if (null === $time) {
            $time = time();
        }

        foreach ($this->toIterator($files) as $file) {
            if (true !== @touch($file, $time, $atime)) {
                throw new IOException(sprintf('Failed to touch %s', $file));
            }
        }
    }

    public function remove($files)
    {
        $files = iterator_to_array($this->toIterator($files));
        $files = array_reverse($files);
        foreach ($files as $file) {
            if (!file_exists($file) && !is_link($file)) {
                continue;
            }

            if (is_dir($file) && !is_link($file)) {
                $this->remove(new \FilesystemIterator($file));

                if (true !== @rmdir($file)) {
                    throw new IOException(sprintf('Failed to remove directory %s', $file));
                }
            } else {
                // https://bugs.php.net/bug.php?id=52176
                if (defined('PHP_WINDOWS_VERSION_MAJOR') && is_dir($file)) {
                    if (true !== @rmdir($file)) {
                        throw new IOException(sprintf('Failed to remove file %s', $file));
                    }
                } else {
                    if (true !== @unlink($file)) {
                        throw new IOException(sprintf('Failed to remove file %s', $file));
                    }
                }
            }
        }
    }

    public function chmod($files, $mode, $umask = 0000, $recursive = false)
    {
        foreach ($this->toIterator($files) as $file) {
            if ($recursive && is_dir($file) && !is_link($file)) {
                $this->chmod(new \FilesystemIterator($file), $mode, $umask, true);
            }
            if (true !== @chmod($file, $mode & ~$umask)) {
                throw new IOException(sprintf('Failed to chmod file %s', $file));
            }
        }
    }

    public function chown($files, $user, $recursive = false)
    {
        foreach ($this->toIterator($files) as $file) {
            if ($recursive && is_dir($file) && !is_link($file)) {
                $this->chown(new \FilesystemIterator($file), $user, true);
            }
            if (is_link($file) && function_exists('lchown')) {
                if (true !== @lchown($file, $user)) {
                    throw new IOException(sprintf('Failed to chown file %s', $file));
                }
            } else {
                if (true !== @chown($file, $user)) {
                    throw new IOException(sprintf('Failed to chown file %s', $file));
                }
            }
        }
    }

    public function chgrp($files, $group, $recursive = false)
    {
        foreach ($this->toIterator($files) as $file) {
            if ($recursive && is_dir($file) && !is_link($file)) {
                $this->chgrp(new \FilesystemIterator($file), $group, true);
            }
            if (is_link($file) && function_exists('lchgrp')) {
                if (true !== @lchgrp($file, $group)) {
                    throw new IOException(sprintf('Failed to chgrp file %s', $file));
                }
            } else {
                if (true !== @chgrp($file, $group)) {
                    throw new IOException(sprintf('Failed to chgrp file %s', $file));
                }
            }
        }
    }

    public function rename($origin, $target)
    {
        // we check that target does not exist
        if (is_readable($target)) {
            throw new IOException(sprintf('Cannot rename because the target "%s" already exist.', $target));
        }

        if (true !== @rename($origin, $target)) {
            throw new IOException(sprintf('Cannot rename "%s" to "%s".', $origin, $target));
        }
    }

    public function symlink($originDir, $targetDir, $copyOnWindows = false)
    {
        if (!function_exists('symlink') && $copyOnWindows) {
            $this->mirror($originDir, $targetDir);

            return;
        }

        $this->mkdir(dirname($targetDir));

        $ok = false;
        if (is_link($targetDir)) {
            if (readlink($targetDir) != $originDir) {
                $this->remove($targetDir);
            } else {
                $ok = true;
            }
        }

        if (!$ok) {
            if (true !== @symlink($originDir, $targetDir)) {
                $report = error_get_last();
                if (is_array($report)) {
                    if (defined('PHP_WINDOWS_VERSION_MAJOR') && false !== strpos($report['message'], 'error code(1314)')) {
                        throw new IOException('Unable to create symlink due to error code 1314: \'A required privilege is not held by the client\'. Do you have the required Administrator-rights?');
                    }
                }
                throw new IOException(sprintf('Failed to create symbolic link from %s to %s', $originDir, $targetDir));
            }
        }
    }

    public function makePathRelative($endPath, $startPath)
    {
        // Normalize separators on windows
        if (defined('PHP_WINDOWS_VERSION_MAJOR')) {
            $endPath = strtr($endPath, '\\', '/');
            $startPath = strtr($startPath, '\\', '/');
        }

        // Split the paths into arrays
        $startPathArr = explode('/', trim($startPath, '/'));
        $endPathArr = explode('/', trim($endPath, '/'));

        // Find for which directory the common path stops
        $index = 0;
        while (isset($startPathArr[$index]) && isset($endPathArr[$index]) && $startPathArr[$index] === $endPathArr[$index]) {
            $index++;
        }

        // Determine how deep the start path is relative to the common path (ie, "web/bundles" = 2 levels)
        $depth = count($startPathArr) - $index;

        // Repeated "../" for each level need to reach the common path
        $traverser = str_repeat('../', $depth);

        $endPathRemainder = implode('/', array_slice($endPathArr, $index));

        // Construct $endPath from traversing to the common path, then to the remaining $endPath
        $relativePath = $traverser . (strlen($endPathRemainder) > 0 ? $endPathRemainder . '/' : '');

        return (strlen($relativePath) === 0) ? './' : $relativePath;
    }

    public function mirror($originDir, $targetDir, \Traversable $iterator = null, $options = array())
    {
        $copyOnWindows = false;
        if (isset($options['copy_on_windows']) && !function_exists('symlink')) {
            $copyOnWindows = $options['copy_on_windows'];
        }

        if (null === $iterator) {
            $flags = $copyOnWindows ? \FilesystemIterator::SKIP_DOTS | \FilesystemIterator::FOLLOW_SYMLINKS : \FilesystemIterator::SKIP_DOTS;
            $iterator = new \RecursiveIteratorIterator(new \RecursiveDirectoryIterator($originDir, $flags), \RecursiveIteratorIterator::SELF_FIRST);
        }

        $targetDir = rtrim($targetDir, '/\\');
        $originDir = rtrim($originDir, '/\\');

        foreach ($iterator as $file) {
            $target = str_replace($originDir, $targetDir, $file->getPathname());

            if (is_dir($file)) {
                $this->mkdir($target);
            } elseif (!$copyOnWindows && is_link($file)) {
                $this->symlink($file, $target);
            } elseif (is_file($file) || ($copyOnWindows && is_link($file))) {
                $this->copy($file, $target, isset($options['override']) ? $options['override'] : false);
            } else {
                throw new IOException(sprintf('Unable to guess "%s" file type.', $file));
            }
        }
    }

    public function isAbsolutePath($file)
    {
        if (strspn($file, '/\\', 0, 1)
            || (strlen($file) > 3 && ctype_alpha($file[0])
                && substr($file, 1, 1) === ':'
                && (strspn($file, '/\\', 2, 1))
            )
            || null !== parse_url($file, PHP_URL_SCHEME)
        ) {
            return true;
        }

        return false;
    }

    private function toIterator($files)
    {
        if (!$files instanceof \Traversable) {
            $files = new \ArrayObject(is_array($files) ? $files : array($files));
        }

        return $files;
    }
}
