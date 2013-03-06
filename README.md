APCu
====

APCu is APC stripped of it's opcode caching in preparation for the deployment of Zend Optimizer+ as the primary solution to opcode caching in future versions of PHP.

APCu has a revised and simplified codebase, by the time the PECL release is available, every part of APCu being used will have recieved review and where necessary or appropriate, changes.

Simplifying and documenting the API of APCu completely removes the barrier to maintenance and development of APCu in the future, and additionally allows us to make optimizations not possible previously because of APC's inherent complexity.

Specific Changes
================

    * appropriate changes (removals) to configuration parameters (complete)
    * appropriate changes to userspace management script (partly complete, could do with a rewrite)
	* revision of APC locking (ongoing, unix-like OS tested & working, more testing to be done)
    * revision of APC's caching API (structures revised and partly documented, functions revised and partly documented, binary functionality requires revision)
    * revision of APC's PHP internals (removals complete, not yet renamed, changes to apc_store(array) behaviour planned)
    * revision of APC's userspace API (removals complete, not yet renamed, changes to iterator complete)
    * revision of APC's shared memory allocator (ongoing)
    * revision of APC's pooling API (complete, no functional changes, documented)
    * installation of SIGUSR1 handler (where possible) to clear cache (complete)
    * documentation of all associated API's (ongoing, first headers then updates/revision/completion of TECHNOTES)

The C api does not retain backward compatibility, anyone relying on APC that will rely on APCu in the future should review the changes as soon as possible and continue to track them.
The userspace PHP api is not likely to retain backward compatibility, though may emulate it at runtime if interest in that is expressed.
