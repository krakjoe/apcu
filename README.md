APCu
====

APCu is an in-memory key-value store for PHP. Keys are of type string and values can be any PHP variables.

APCu only supports userland caching of variables.

APCu is APC stripped of opcode caching.
See [APCu Backwards Compatibility Module](https://github.com/krakjoe/apcu-bc) which provides a drop in replacement for APC.

[![Build Status](https://travis-ci.org/krakjoe/apcu.svg?branch=master)](https://travis-ci.org/krakjoe/apcu)
[![Build status](https://ci.appveyor.com/api/projects/status/om63glh4g24gi1p9/branch/master?svg=true)](https://ci.appveyor.com/project/krakjoe/apcu/branch/master)

Documentation
============

APCu documentation can be found on [php.net](http://php.net/apcu).

Reporting Bugs
=============

If you believe you have found a bug in APCu, please open an issue: Include in your report *minimal, executable, reproducing code*.

Minimal: reduce your problem to the smallest amount of code possible; This helps with hunting the bug, but also it helps with integration and regression testing once the bug is fixed.

Executable: include all the information required to execute the example code, code snippets are not helpful.

Reproducing: some bugs don't show themselves on every execution, that's fine, mention that in the report and give an idea of how often you encounter the bug.

__It is impossible to help without reproducing code, bugs that are opened without reproducing code will be closed.__

Please include version and operating system information in your report.
