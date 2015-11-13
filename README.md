PHP7
===

PHP7 is coming and APCu is coming with us ... 

Shiny Things
===========

```php
/**
* Atomically fetch or generate a cache entry
**/
function apcu_entry(string key, callable generator, int ttl) : mixed;
```

  * Address inadequacy of stampede protection. 
  * Introduce a way to atomically warm a cache.
  * Provide a simpler interface to the cache.

```apcu_entry``` shall first acquire an exclusive lock on the cache.

If the entry identified by ```key``` *exists* and *has not timed out*, it's value shall be returned.

If the entry identified by ```key``` *does not exist* or *has timed out*, ```generator(key)``` shall 
be called and the return value cached (identified by ```key```) with the optionally specified ```ttl```.

```apcu_entry``` shall then release the exclusive lock on the cache.

Internal Changes
--------------

APC(u) did break a basic rule, it's always done this and for the most part it works: It acquires a read lock and performs writes.

There was a frustrating number of people who experience problems with caching that we are not able to investigate, this is a tell-tell sign that you have concurrency bugs ...

APCu >= 5.1.0 introduces internal changes that should avoid races, it might also change the performance characteristics of the cache.

APCu
====

APCu is userland caching: APC stripped of opcode caching in preparation for the deployment of Zend Optimizer+ as the primary solution to opcode caching in future versions of PHP.

APCu has a revised and simplified codebase, by the time the PECL release is available, every part of APCu being used will have received review and where necessary or appropriate, changes.

Simplifying and documenting the API of APCu completely removes the barrier to maintenance and development of APCu in the future, and additionally allows us to make optimizations not possible previously because of APC's inherent complexity.

APCu only supports userland caching (and dumping) of variables, providing an upgrade path for the future. When O+ takes over, many will be tempted to use 3rd party solutions to userland caching, possibly even distributed solutions; this would be a grave error. The tried and tested APC codebase provides far superior support for local storage of PHP variables.

[![Build Status](https://travis-ci.org/krakjoe/apcu.svg?branch=seven)](https://travis-ci.org/krakjoe/apcu)

Specific Changes
================

    * appropriate changes (removals) to configuration parameters (complete)
    * appropriate changes to userspace management script (complete)
	* revision of APC locking (complete)
    * revision of APC's caching API (complete)
    * revision of APC's PHP internals (complete)
    * revision of APC's userspace API (complete)
    * revision of APC's shared memory allocator (complete)
    * revision of APC's pooling API (complete, no functional changes, documented)
    * installation of SIGUSR1 handler (where possible) to clear cache (complete)
    * format API headers so they can be referenced externally (complete, see apcue)
    * apc.smart allows runtime adjustable control over how the cache is purged (complete)
    * documentation of all associated API's (ongoing, mostly complete, all headers and source file documented and commented)

Still to Do:
============

	* test/stabilize

The C api does not retain backward compatibility, anyone relying on APC that will rely on APCu in the future should review the changes as soon as possible and continue to track them.
The PHP api will retain compatibility with APC, as will common configuration options, providing a drop in replacement.
