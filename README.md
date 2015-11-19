APCu
====

APCu is userland caching: APC stripped of opcode caching.

APCu only supports userland caching of variables, and can provide a drop in replacement for APC.

[![Build Status](https://travis-ci.org/krakjoe/apcu.svg?branch=master)](https://travis-ci.org/krakjoe/apcu)

Documentation
============

APCu documentation can be found on [php.net](http://php.net/apcu).

PHP7
====

APC(u) did break a basic rule, it's always done this and for the most part it works: It acquires a read lock and performs writes.

There was a frustrating number of people who experience problems with caching that we are not able to investigate, this is a tell-tell sign that you have concurrency bugs ...

APCu >= 5.1.0 (PHP 7+) introduces internal changes that should avoid races, it might also change the performance characteristics of the cache.

Reporting Bugs
=============

If you believe you have found a bug in APCu, please open an issue: Include in your report *minimal, executable, reproducing code*.

Minimal: reduce your problem to the smallest amount of code possible; This helps with hunting the bug, but also it helps with integration and regression testing once the bug is fixed.

Executable: include all the information required to execute the example code, code snippets are not helpful.

Reproducing: some bugs don't show themselves on every execution, that's fine, mention that in the report and give an idea of how often you encounter the bug.

__It is impossible to help without reproducing code, bugs that are opened without reproducing code will be closed.__

Please include version and operating system information in your report.
